/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include <cstdalign>
#pragma comment(lib, "Dokan2.lib")

namespace
{
	void InitDokany()
	{
		Dokany2::DokanInit(nullptr);
	}
	void UninitDokany()
	{
		// If VFS fails, it will uninitialize itself. I have no way to tell if it's failed, so just ignore that.
		__try
		{
			Dokany2::DokanShutdown();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// DOKAN_EXCEPTION_NOT_INITIALIZED was thrown
		}
	}

	bool operator==(const LUID& left, const LUID& right) noexcept
	{
		return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
	}
}

namespace KxVFS
{
	KxDynamicStringW FileSystemService::GetLibraryVersion()
	{
		return L"2.0a";
	}
	KxDynamicStringW FileSystemService::GetDokanyVersion()
	{
		// Return 2.0 for now because this is the version of Dokany used in KxVFS.
		// It seems that 'DOKAN_VERSION' constant hasn't updated (maybe because 2.x still beta).
		#undef DOKAN_VERSION
		#define DOKAN_VERSION 200

		const KxDynamicStringW temp = KxDynamicStringW::Format(L"%d", static_cast<int>(DOKAN_VERSION));
		return KxDynamicStringW::Format(L"%c.%c.%c", temp[0], temp[1], temp[2]);
	}

	bool FileSystemService::AddSeSecurityNamePrivilege()
	{
		LUID securityLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_SECURITY_NAME, &securityLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		LUID restoreLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_RESTORE_NAME, &restoreLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		LUID backupLuid = {};
		if (!::LookupPrivilegeValueW(nullptr, SE_BACKUP_NAME, &backupLuid))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		constexpr size_t privilegesSize = sizeof(TOKEN_PRIVILEGES) + (sizeof(LUID_AND_ATTRIBUTES) * 2);
		alignas(TOKEN_PRIVILEGES) uint8_t tokenPrivilegesBuffer[privilegesSize] = {};
		alignas(TOKEN_PRIVILEGES) uint8_t oldTokenPrivilegesBuffer[privilegesSize] = {};
		TOKEN_PRIVILEGES& tokenPrivileges = *reinterpret_cast<TOKEN_PRIVILEGES*>(tokenPrivilegesBuffer);
		TOKEN_PRIVILEGES& oldTokenPrivileges = *reinterpret_cast<TOKEN_PRIVILEGES*>(oldTokenPrivilegesBuffer);

		tokenPrivileges.PrivilegeCount = 3;
		tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[0].Luid = securityLuid;
		tokenPrivileges.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[1].Luid = restoreLuid;
		tokenPrivileges.Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;
		tokenPrivileges.Privileges[2].Luid = backupLuid;

		GenericHandle processToken;
		if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, &processToken))
		{
			if (::GetLastError() != ERROR_SUCCESS)
			{
				return false;
			}
		}

		DWORD returnLength = 0;
		::AdjustTokenPrivileges(processToken, FALSE, &tokenPrivileges, static_cast<DWORD>(privilegesSize), &oldTokenPrivileges, &returnLength);
		if (::GetLastError() != ERROR_SUCCESS)
		{
			return false;
		}

		bool securityPrivilegePresent = false;
		bool restorePrivilegePresent = false;
		for (size_t i = 0; i < oldTokenPrivileges.PrivilegeCount && (!securityPrivilegePresent || !restorePrivilegePresent); i++)
		{
			if (oldTokenPrivileges.Privileges[i].Luid == securityLuid)
			{
				securityPrivilegePresent = true;
			}
			else if (oldTokenPrivileges.Privileges[i].Luid == restoreLuid)
			{
				restorePrivilegePresent = true;
			}
		}
		return true;
	}

	FileSystemService::FileSystemService(KxDynamicStringRefW serviceName)
		:m_ServiceName(serviceName), m_ServiceManager(ServiceAccess::Start|ServiceAccess::Stop|ServiceAccess::QueryStatus|ServiceAccess::ChangeConfig)
	{
		m_DriverService.Open(m_ServiceManager, serviceName, ServiceAccess::All, AccessRights::Delete);
		m_HasSeSecurityNamePrivilege = AddSeSecurityNamePrivilege();

		InitDokany();
	}
	FileSystemService::~FileSystemService()
	{
		UninitDokany();
	}

	bool FileSystemService::IsOK() const
	{
		return m_ServiceManager.IsOK();
	}
	KxDynamicStringRefW FileSystemService::GetServiceName() const
	{
		return m_ServiceName;
	}
	bool FileSystemService::IsInstalled() const
	{
		return IsOK() && m_DriverService.IsValid();
	}
	bool FileSystemService::IsStarted() const
	{
		return m_DriverService.GetStatus() == ServiceStatus::Running;
	}

	bool FileSystemService::Start()
	{
		return m_DriverService.Start();
	}
	bool FileSystemService::Stop()
	{
		return m_DriverService.Stop();
	}
	bool FileSystemService::Install(KxDynamicStringRefW binaryPath, KxDynamicStringRefW displayName, KxDynamicStringRefW description)
	{
		if (Utility::IsFileExist(binaryPath))
		{
			// Create new service or reconfigure existing
			constexpr ServiceStartType startMode = ServiceStartType::OnDemand;

			if (m_DriverService.IsValid())
			{
				if (m_DriverService.Reconfigure(m_ServiceManager, startMode, binaryPath))
				{
					m_DriverService.SetDescription(description);
					return true;
				}
				return false;
			}
			else
			{
				return m_DriverService.Create(m_ServiceManager,
											  startMode,
											  binaryPath,
											  m_ServiceName,
											  !displayName.empty() ? displayName : m_ServiceName,
											  description
				);
			}
		}
		return false;
	}
	bool FileSystemService::Uninstall()
	{
		return m_DriverService.Delete();
	}

	void FileSystemService::AddActiveFS(IFileSystem& fileSystem)
	{
		m_ActiveFileSystems.remove(&fileSystem);
		m_ActiveFileSystems.push_back(&fileSystem);
	}
	void FileSystemService::RemoveActiveFS(IFileSystem& fileSystem)
	{
		m_ActiveFileSystems.remove(&fileSystem);
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
