/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility/Wow64RedirectionDisabler.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include <exception>
#pragma comment(lib, "Dokan2.lib")

namespace
{
	KxVFS::FileSystemService* g_FileSystemServiceInstance = nullptr;

	bool InitDokany(Dokany2::DOKAN_LOG_CALLBACKS* logCallbacks = nullptr)
	{
		using namespace KxVFS;

		__try
		{
			Dokany2::DokanInit(nullptr, logCallbacks);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			if (g_FileSystemServiceInstance)
			{
				const uint32_t exceptionCode = GetExceptionCode();
				const KxDynamicStringRefW exceptionMessage = Utility::ExceptionCodeToString(exceptionCode);

				KxVFS_Log(LogLevel::Fatal, L"Fatal exception '%1 (%2)' occurred while initializing Dokany", exceptionMessage, exceptionCode);
			}
			return false;
		}
	}
	bool UninitDokany()
	{
		using namespace KxVFS;

		// If VFS fails, it will uninitialize itself. I have no way to tell if it's failed, so just ignore that.
		__try
		{
			Dokany2::DokanShutdown();
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			if (g_FileSystemServiceInstance)
			{
				const uint32_t exceptionCode = GetExceptionCode();
				const KxDynamicStringRefW exceptionMessage = Utility::ExceptionCodeToString(exceptionCode);

				KxVFS_Log(LogLevel::Fatal, L"Fatal exception '%1 (%2)' occurred in the process of Dokany shutdown", exceptionMessage, exceptionCode);
			}
			return false;
		}
	}

	bool operator==(const LUID& left, const LUID& right) noexcept
	{
		return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
	}
	KxVFS::KxDynamicStringW ProcessDokanyLogString(KxVFS::KxDynamicStringRefW logString)
	{
		using namespace KxVFS;

		KxDynamicStringW temp = logString;
		temp.trim_linebreak_chars();
		temp.trim_space_chars();

		KxDynamicStringW fullLogString = L"<Dokany> ";
		fullLogString += temp;
		fullLogString.trim_space_chars();

		return fullLogString;
	}
}

namespace KxVFS
{
	FileSystemService* FileSystemService::GetInstance()
	{
		return g_FileSystemServiceInstance;
	}

	KxDynamicStringW FileSystemService::GetLibraryVersion()
	{
		return L"2.0";
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

	KxDynamicStringW FileSystemService::GetDokanyDefaultServiceName()
	{
		return DOKAN_DRIVER_SERVICE;
	}
	KxDynamicStringW FileSystemService::GetDokanyDefaultDriverPath()
	{
		// There's a 'DOKAN_DRIVER_FULL_PATH' constant in Dokany control app, but it's defined in the .cpp file
		return Utility::ExpandEnvironmentStrings(L"%SystemRoot%\\system32\\drivers\\Dokan" DOKAN_MAJOR_API_VERSION L".sys");
	}
	bool FileSystemService::IsDokanyDefaultInstallPresent()
	{
		ServiceManager serviceManager(ServiceAccess::QueryStatus);
		if (serviceManager)
		{
			ServiceHandle dokanyService;
			if (dokanyService.Open(serviceManager, GetDokanyDefaultServiceName(), ServiceAccess::QueryStatus))
			{
				KxDynamicStringW driverPath = GetDokanyDefaultDriverPath();

				Wow64RedirectionDisabler disable;
				return Utility::IsFileExist(driverPath);
			}
		}
		return false;
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
	bool FileSystemService::InitDriver()
	{
		if (m_DriverService.Open(m_ServiceManager, m_ServiceName, ServiceAccess::All, AccessRights::Delete))
		{
			if (!m_IsFSInitialized)
			{
				if (Setup::EnableLog)
				{
					Dokany2::DOKAN_LOG_CALLBACKS logCallbacks = {};
					logCallbacks.DbgPrint = [](const char* logString)
					{
						KxDynamicStringW logStringW = KxDynamicStringA::to_utf16(logString, KxDynamicStringA::npos, CP_ACP);
						KxDynamicStringW fullLogString = ProcessDokanyLogString(logStringW);

						g_FileSystemServiceInstance->GetLogger().Log(LogLevel::Info, fullLogString);
						return FALSE;
					};
					logCallbacks.DbgPrintW = [](const wchar_t* logString)
					{
						KxDynamicStringW fullLogString = ProcessDokanyLogString(logString);
						g_FileSystemServiceInstance->GetLogger().Log(LogLevel::Info, logString);
						return FALSE;
					};

					m_IsFSInitialized = InitDokany(&logCallbacks);
				}
				else
				{
					m_IsFSInitialized = InitDokany();
				}
			}
			return m_IsFSInitialized;
		}
		return false;
	}

	FileSystemService::FileSystemService(KxDynamicStringRefW serviceName)
		:m_ServiceName(serviceName),
		m_ServiceManager(ServiceAccess::Start|ServiceAccess::Stop|ServiceAccess::QueryStatus|ServiceAccess::ChangeConfig),
		m_HasSeSecurityNamePrivilege(AddSeSecurityNamePrivilege())
	{
		// Init instance pointer
		if (g_FileSystemServiceInstance)
		{
			throw std::logic_error("KxVFS: an instance of 'FileSystemService' already created");
		}
		g_FileSystemServiceInstance = this;

		// Init default logger
		if (Setup::Configuration::Debug)
		{
			m_ChainLogger.AddLogger(m_StdOutLogger);
		}
		m_ChainLogger.AddLogger(m_DebugLogger);

		// Init driver
		if (!serviceName.empty())
		{
			InitDriver();
		}
	}
	FileSystemService::~FileSystemService()
	{
		if (m_IsFSInitialized)
		{
			UninitDokany();
			m_IsFSInitialized = false;
		}

		g_FileSystemServiceInstance = nullptr;
	}

	bool FileSystemService::IsOK() const
	{
		return m_ServiceManager.IsOK();
	}
	bool FileSystemService::InitService(KxDynamicStringRefW name)
	{
		if (m_ServiceName.empty())
		{
			m_ServiceName = name;
			return InitDriver();
		}
		return false;
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
			constexpr ServiceStartMode startMode = ServiceStartMode::OnDemand;

			if (m_DriverService.IsValid())
			{
				if (m_DriverService.SetConfig(m_ServiceManager, binaryPath, ServiceType::FileSystemDriver, startMode, ServiceErrorControl::Ignore))
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

	bool FileSystemService::UseDefaultDokanyInstallation()
	{
		KxDynamicStringW binaryPath = GetDokanyDefaultDriverPath();
		if (Wow64RedirectionDisabler disable; Utility::IsFileExist(binaryPath))
		{
			m_ServiceName = GetDokanyDefaultServiceName();
			return InitDriver();
		}
		return false;
	}
	bool FileSystemService::IsUsingDefaultDokanyInstallation() const
	{
		auto config = m_DriverService.GetConfig();
		return config && config->DisplayName == GetDokanyDefaultServiceName();
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
