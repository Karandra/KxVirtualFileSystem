/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxVFSService.h"
#include "KxVFSBase.h"
#include "KxVFSIncludeDokan.h"
#include "Utility/KxVFSUtility.h"
#pragma comment(lib, "Dokan2.lib")

const WCHAR* KxVFSService::GetLibraryVersion()
{
	return L"1.1";
}
const WCHAR* KxVFSService::GetDokanVersion()
{
	static std::wstring ms_DokanVersion;
	if (ms_DokanVersion.empty())
	{
		WCHAR temp1[8] = {0};
		WCHAR temp2[8] = {0};
		wsprintfW(temp1, L"%d", DOKAN_VERSION);
		int size = wsprintfW(temp2, L"%c.%c.%c", temp1[0], temp1[1], temp1[2]);

		ms_DokanVersion.reserve(size);
		ms_DokanVersion = temp2;
	}
	return ms_DokanVersion.c_str();
}

DWORD KxVFSService::GetServiceStatus(SC_HANDLE serviceHandle)
{
	SERVICE_STATUS_PROCESS status = {0};
	DWORD reqSize = 0;
	::QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (BYTE*)&status, sizeof(status), &reqSize);

	return status.dwCurrentState;
}
SC_HANDLE KxVFSService::CreateService(SC_HANDLE serviceManagerHandle, const WCHAR* driverPath, const WCHAR* serviceName, const WCHAR* displayName, const WCHAR* description)
{
	SC_HANDLE serviceHandle = CreateServiceW
	(
		serviceManagerHandle,
		serviceName,
		displayName,
		SERVICE_ALL_ACCESS,
		SERVICE_FILE_SYSTEM_DRIVER,
		ServiceStartType,
		SERVICE_ERROR_IGNORE,
		driverPath,
		NULL, NULL, NULL, NULL, NULL
	);

	if (serviceHandle)
	{
		SetServiceDescription(serviceHandle, description);
	}
	return serviceHandle;
}
bool KxVFSService::ReconfigureService(SC_HANDLE serviceHandle, const WCHAR* driverPath, const WCHAR* displayName, const WCHAR* description)
{
	bool b1 = ChangeServiceConfigW
	(
		serviceHandle,
		SERVICE_FILE_SYSTEM_DRIVER,
		ServiceStartType,
		SERVICE_ERROR_IGNORE,
		driverPath,
		NULL, NULL, NULL, NULL, NULL, NULL
	);
	bool b2 = SetServiceDescription(serviceHandle, description);

	return b1 && b2;
}
bool KxVFSService::SetServiceDescription(SC_HANDLE serviceHandle, const WCHAR* description)
{
	SERVICE_DESCRIPTION desc = {0};
	desc.lpDescription = const_cast<LPWSTR>(description);
	return ChangeServiceConfig2W(serviceHandle, SERVICE_CONFIG_DESCRIPTION, &desc);
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

KxVFSService::KxVFSService(const WCHAR* serviceName)
	:m_ServiceName(serviceName), m_ServiceManager(SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG)
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
	SERVICE_STATUS status = {0};
	return ControlService(m_ServiceHandle, SERVICE_STOP, &status);
}
bool KxVFSService::Install(const WCHAR* driverPath, const WCHAR* displayName, const WCHAR* description)
{
	if (GetFileAttributesW(driverPath) != INVALID_FILE_ATTRIBUTES)
	{
		// Create new service or reconfigure existing
		if (m_ServiceHandle)
		{
			return ReconfigureService(m_ServiceHandle, driverPath, displayName, description);
		}
		else
		{
			m_ServiceHandle = CreateService(m_ServiceManager, driverPath, m_ServiceName.c_str(), displayName ? displayName : m_ServiceName.c_str(), description ? description : L"");
			return m_ServiceHandle != NULL;
		}
	}
	return false;
}
bool KxVFSService::Uninstall()
{
	return ::DeleteService(m_ServiceHandle);
}

void KxVFSService::AddVFS(KxVFSBase* vfs)
{
	GetVFSList().emplace_back(vfs);
}
void KxVFSService::RemoveVFS(KxVFSBase* vfs)
{
	GetVFSList().remove(vfs);
}

BOOL WINAPI DllMain(HMODULE moduleHandle, DWORD event, LPVOID pReserved)
{
	switch (event)
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
