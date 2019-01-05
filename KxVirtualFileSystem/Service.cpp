/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeDokan.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Utility.h"
#pragma comment(lib, "Dokan2.lib")

namespace
{
	void InitDokany()
	{
		Dokany2::DokanInit(nullptr);
	}
	void UninitDokany()
	{
		// If VFS fails, it will uninitialize itself. I have no way to tell if it failed, so just ignore that.
		__try
		{
			Dokany2::DokanShutdown();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// DOKAN_EXCEPTION_NOT_INITIALIZED was thrown
		}
	}
}

namespace KxVFS
{
	KxDynamicStringW Service::GetLibraryVersion()
	{
		return L"1.4.1";
	}
	KxDynamicStringW Service::GetDokanyVersion()
	{
		// Return 2.0 for now because this is the used version of Dokany.
		// It seems that 'DOKAN_VERSION' constant hasn't updated (maybe because it's beta).
		#undef DOKAN_VERSION
		#define DOKAN_VERSION 200

		const KxDynamicStringW temp = KxDynamicStringW::Format(L"%d", static_cast<int>(DOKAN_VERSION));
		return KxDynamicStringW::Format(L"%c.%c.%c", temp[0], temp[1], temp[2]);
	}

	DWORD Service::GetServiceStatus(SC_HANDLE serviceHandle)
	{
		SERVICE_STATUS_PROCESS status = {0};
		DWORD reqSize = 0;
		::QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (BYTE*)&status, sizeof(status), &reqSize);

		return status.dwCurrentState;
	}
	SC_HANDLE Service::CreateService(SC_HANDLE serviceManagerHandle, KxDynamicStringRefW driverPath, KxDynamicStringRefW serviceName, KxDynamicStringRefW displayName, KxDynamicStringRefW description)
	{
		SC_HANDLE serviceHandle = CreateServiceW
		(
			serviceManagerHandle,
			serviceName.data(),
			displayName.data(),
			SERVICE_ALL_ACCESS,
			SERVICE_FILE_SYSTEM_DRIVER,
			ServiceStartType,
			SERVICE_ERROR_IGNORE,
			driverPath.data(),
			nullptr, nullptr, nullptr, nullptr, nullptr
		);

		if (serviceHandle)
		{
			SetServiceDescription(serviceHandle, description);
		}
		return serviceHandle;
	}
	bool Service::ReconfigureService(SC_HANDLE serviceHandle, KxDynamicStringRefW driverPath, KxDynamicStringRefW displayName, KxDynamicStringRefW description)
	{
		bool b1 = ChangeServiceConfigW
		(
			serviceHandle,
			SERVICE_FILE_SYSTEM_DRIVER,
			ServiceStartType,
			SERVICE_ERROR_IGNORE,
			driverPath.data(),
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
		);
		bool b2 = SetServiceDescription(serviceHandle, description);

		return b1 && b2;
	}
	bool Service::SetServiceDescription(SC_HANDLE serviceHandle, KxDynamicStringRefW description)
	{
		SERVICE_DESCRIPTION desc = {0};
		desc.lpDescription = const_cast<LPWSTR>(description.data());
		return ChangeServiceConfig2W(serviceHandle, SERVICE_CONFIG_DESCRIPTION, &desc);
	}

	bool Service::AddSeSecurityNamePrivilege()
	{
		DWORD errorCode;

		LUID securityLuid;
		if (!LookupPrivilegeValue(0, SE_SECURITY_NAME, &securityLuid))
		{
			errorCode = GetLastError();
			if (errorCode != ERROR_SUCCESS)
			{
				return FALSE;
			}
		}

		LUID restoreLuid;
		if (!LookupPrivilegeValue(0, SE_RESTORE_NAME, &restoreLuid))
		{
			errorCode = GetLastError();
			if (errorCode != ERROR_SUCCESS)
			{
				return FALSE;
			}
		}

		LUID backupLuid;
		if (!LookupPrivilegeValue(0, SE_BACKUP_NAME, &backupLuid))
		{
			errorCode = GetLastError();
			if (errorCode != ERROR_SUCCESS)
			{
				return FALSE;
			}
		}

		size_t privSize = sizeof(TOKEN_PRIVILEGES) + (sizeof(LUID_AND_ATTRIBUTES) * 2);
		PTOKEN_PRIVILEGES privs = (PTOKEN_PRIVILEGES)malloc(privSize);
		PTOKEN_PRIVILEGES oldPrivs = (PTOKEN_PRIVILEGES)malloc(privSize);

		KxCallAtScopeExit onExit([privs, oldPrivs]()
		{
			free(privs);
			free(oldPrivs);
		});

		privs->PrivilegeCount = 3;
		privs->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		privs->Privileges[0].Luid = securityLuid;
		privs->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
		privs->Privileges[1].Luid = restoreLuid;
		privs->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;
		privs->Privileges[2].Luid = backupLuid;

		HANDLE processToken = nullptr;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &processToken))
		{
			errorCode = GetLastError();
			if (errorCode != ERROR_SUCCESS)
			{
				return FALSE;
			}
		}

		DWORD retSize;
		AdjustTokenPrivileges(processToken, FALSE, privs, (DWORD)privSize, oldPrivs, &retSize);
		errorCode = GetLastError();
		CloseHandle(processToken);

		if (errorCode != ERROR_SUCCESS)
		{
			return FALSE;
		}

		BOOL securityPrivPresent = FALSE;
		BOOL restorePrivPresent = FALSE;
		for (size_t i = 0; i < oldPrivs->PrivilegeCount && (!securityPrivPresent || !restorePrivPresent); i++)
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
		return TRUE;
	}

	Service::Service(KxDynamicStringRefW serviceName)
		:m_ServiceName(serviceName), m_ServiceManager(SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG)
	{
		m_ServiceHandle = ::OpenServiceW(m_ServiceManager, m_ServiceName.c_str(), SERVICE_ALL_ACCESS|DELETE);
		m_HasSeSecurityNamePrivilege = AddSeSecurityNamePrivilege();

		InitDokany();
	}
	Service::~Service()
	{
		CloseServiceHandle(m_ServiceHandle);
		UninitDokany();
	}

	bool Service::IsOK() const
	{
		return m_ServiceManager.IsOK();
	}
	KxDynamicStringRefW Service::GetServiceName() const
	{
		return m_ServiceName;
	}
	bool Service::IsInstalled() const
	{
		return IsOK() && m_ServiceHandle != nullptr;
	}
	bool Service::IsStarted() const
	{
		return GetServiceStatus(m_ServiceHandle) == SERVICE_RUNNING;
	}

	bool Service::Start()
	{
		return ::StartServiceW(m_ServiceHandle, 0, nullptr);
	}
	bool Service::Stop()
	{
		SERVICE_STATUS status = {0};
		return ControlService(m_ServiceHandle, SERVICE_STOP, &status);
	}
	bool Service::Install(KxDynamicStringRefW driverPath, KxDynamicStringRefW displayName, KxDynamicStringRefW description)
	{
		if (Utility::IsFileExist(driverPath))
		{
			// Create new service or reconfigure existing
			if (m_ServiceHandle)
			{
				return ReconfigureService(m_ServiceHandle, driverPath, displayName, description);
			}
			else
			{
				m_ServiceHandle = CreateService(m_ServiceManager,
												driverPath,
												m_ServiceName.c_str(),
												!displayName.empty() ? displayName : m_ServiceName,
												!description.empty() ? description : KxDynamicStringRefW()
				);
				return m_ServiceHandle != nullptr;
			}
		}
		return false;
	}
	bool Service::Uninstall()
	{
		return ::DeleteService(m_ServiceHandle);
	}

	void Service::AddFS(AbstractFS* vfs)
	{
		m_VFSList.emplace_back(vfs);
	}
	void Service::RemoveFS(AbstractFS* vfs)
	{
		m_VFSList.remove(vfs);
	}
}

BOOL WINAPI DllMain(HMODULE moduleHandle, DWORD event, LPVOID reserved)
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
		case DLL_THREAD_ATTACH:
		{
			break;
		}
		case DLL_THREAD_DETACH:
		{
			break;
		}
	};
	return true;
}
