#include "KxVirtualFileSystem.h"
#include "KxVFSService.h"
#include "KxVFSBase.h"
#include "KxVFSIncludeDokan.h"
#include "Utility/KxVFSUtility.h"
#pragma comment(lib, "Dokan2.lib")

const WCHAR* KxVFSService::GetLibraryVersion()
{
	static std::wstring ms_DokanVersion;
	if (ms_DokanVersion.empty())
	{
		WCHAR sTemp1[8] = {0};
		WCHAR sTemp2[8] = {0};
		wsprintfW(sTemp1, L"%d", DOKAN_VERSION);
		wsprintfW(sTemp2, L"%c.%c.%c", sTemp1[0], sTemp1[1], sTemp1[2]);
		ms_DokanVersion = sTemp2;
	}
	return ms_DokanVersion.c_str();
}

DWORD KxVFSService::GetServiceStatus(SC_HANDLE hService)
{
	SERVICE_STATUS_PROCESS tStatus = {0};
	DWORD nReqSize = 0;
	::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (BYTE*)&tStatus, sizeof(tStatus), &nReqSize);

	return tStatus.dwCurrentState;
}
SC_HANDLE KxVFSService::CreateService(SC_HANDLE hServiceManager, const WCHAR* sDriverPath, const WCHAR* sServiceName, const WCHAR* sDisplayName, const WCHAR* sDescription)
{
	SC_HANDLE hService = CreateServiceW
	(
		hServiceManager,
		sServiceName,
		sDisplayName,
		SERVICE_ALL_ACCESS,
		SERVICE_FILE_SYSTEM_DRIVER,
		ServiceStartType,
		SERVICE_ERROR_IGNORE,
		sDriverPath,
		NULL, NULL, NULL, NULL, NULL
	);

	if (hService)
	{
		SetServiceDescription(hService, sDescription);
	}
	return hService;
}
bool KxVFSService::ReconfigureService(SC_HANDLE hService, const WCHAR* sDriverPath, const WCHAR* sDisplayName, const WCHAR* sDescription)
{
	bool b1 = ChangeServiceConfigW
	(
		hService,
		SERVICE_FILE_SYSTEM_DRIVER,
		ServiceStartType,
		SERVICE_ERROR_IGNORE,
		sDriverPath,
		NULL, NULL, NULL, NULL, NULL, NULL
	);
	bool b2 = SetServiceDescription(hService, sDescription);

	return b1 && b2;
}
bool KxVFSService::SetServiceDescription(SC_HANDLE hService, const WCHAR* sDescription)
{
	SERVICE_DESCRIPTION tDesc = {0};
	tDesc.lpDescription = const_cast<LPWSTR>(sDescription);
	return ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &tDesc);
}

bool KxVFSService::AddSeSecurityNamePrivilege()
{
	HANDLE token = 0;
	DWORD err;
	LUID securityLuid;
	LUID restoreLuid;
	LUID backupLuid;

	if (!LookupPrivilegeValue(0, SE_SECURITY_NAME, &securityLuid))
	{
		err = GetLastError();
		if (err != ERROR_SUCCESS)
		{
			return FALSE;
		}
	}

	if (!LookupPrivilegeValue(0, SE_RESTORE_NAME, &restoreLuid))
	{
		err = GetLastError();
		if (err != ERROR_SUCCESS)
		{
			return FALSE;
		}
	}

	if (!LookupPrivilegeValue(0, SE_BACKUP_NAME, &backupLuid))
	{
		err = GetLastError();
		if (err != ERROR_SUCCESS)
		{
			return FALSE;
		}
	}

	size_t privSize = sizeof(TOKEN_PRIVILEGES) + (sizeof(LUID_AND_ATTRIBUTES) * 2);
	PTOKEN_PRIVILEGES privs = (PTOKEN_PRIVILEGES)malloc(privSize);
	PTOKEN_PRIVILEGES oldPrivs = (PTOKEN_PRIVILEGES)malloc(privSize);

	privs->PrivilegeCount = 3;
	privs->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	privs->Privileges[0].Luid = securityLuid;
	privs->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
	privs->Privileges[1].Luid = restoreLuid;
	privs->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;
	privs->Privileges[2].Luid = backupLuid;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
	{
		err = GetLastError();
		if (err != ERROR_SUCCESS)
		{
			free(privs);
			free(oldPrivs);
			return FALSE;
		}
	}

	DWORD retSize;
	AdjustTokenPrivileges(token, FALSE, privs, (DWORD)privSize, oldPrivs, &retSize);
	err = GetLastError();
	CloseHandle(token);

	if (err != ERROR_SUCCESS)
	{
		free(privs);
		free(oldPrivs);
		return FALSE;
	}

	BOOL securityPrivPresent = FALSE;
	BOOL restorePrivPresent = FALSE;

	for (unsigned int i = 0; i < oldPrivs->PrivilegeCount && (!securityPrivPresent || !restorePrivPresent); i++)
	{
		if (oldPrivs->Privileges[i].Luid.HighPart == securityLuid.HighPart && oldPrivs->Privileges[i].Luid.LowPart == securityLuid.LowPart)
		{
			securityPrivPresent = TRUE;
		}
		else if (oldPrivs->Privileges[i].Luid.HighPart == restoreLuid.HighPart && oldPrivs->Privileges[i].Luid.LowPart == restoreLuid.LowPart)
		{
			restorePrivPresent = TRUE;
		}
	}

	free(privs);
	free(oldPrivs);
	return TRUE;
}

KxVFSService::KxVFSService(const WCHAR* sServiceName)
	:m_ServiceName(sServiceName), m_ServiceManager(SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG)
{
	m_ServiceHandle = ::OpenServiceW(m_ServiceManager, m_ServiceName.c_str(), SERVICE_ALL_ACCESS|DELETE);
	m_HasSeSecurityNamePrivilege = AddSeSecurityNamePrivilege();

	DokanInit(NULL);
}
KxVFSService::~KxVFSService()
{
	CloseServiceHandle(m_ServiceHandle);
	DokanShutdown();
}

bool KxVFSService::IsOK() const
{
	return m_ServiceManager.IsOK();
}
const WCHAR* KxVFSService::GetServiceName() const
{
	return m_ServiceName.c_str();
}
bool KxVFSService::IsInstalled() const
{
	return IsOK() && m_ServiceHandle != NULL;
}
bool KxVFSService::IsStarted() const
{
	return GetServiceStatus(m_ServiceHandle) == SERVICE_RUNNING;
}

bool KxVFSService::Start()
{
	return ::StartServiceW(m_ServiceHandle, 0, NULL);
}
bool KxVFSService::Stop()
{
	SERVICE_STATUS tStatus = {0};
	return ControlService(m_ServiceHandle, SERVICE_STOP, &tStatus);
}
bool KxVFSService::Install(const WCHAR* sDriverPath, const WCHAR* sDisplayName, const WCHAR* sDescription)
{
	bool bSuccess = false;
	if (GetFileAttributesW(sDriverPath) != INVALID_FILE_ATTRIBUTES)
	{
		// Create new service or reconfigure existing
		if (m_ServiceHandle)
		{
			return ReconfigureService(m_ServiceHandle, sDriverPath, sDisplayName, sDescription);
		}
		else
		{
			m_ServiceHandle = CreateService(m_ServiceManager, sDriverPath, m_ServiceName.c_str(), sDisplayName ? sDisplayName : m_ServiceName.c_str(), sDescription ? sDescription : L"");
			return m_ServiceHandle != NULL;
		}
	}
	return false;
}
bool KxVFSService::Uninstall()
{
	return ::DeleteService(m_ServiceHandle);
}

void KxVFSService::AddVFS(KxVFSBase* pVFS)
{
	GetVFSList().emplace_back(pVFS);
}
void KxVFSService::RemoveVFS(KxVFSBase* pVFS)
{
	GetVFSList().remove(pVFS);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD nReason, LPVOID pReserved)
{
	switch (nReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			break;
		}
	};
	return TRUE;
}
