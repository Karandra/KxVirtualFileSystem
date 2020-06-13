#pragma once
#include "Common.hpp"
#include "Misc/IncludeWindows.h"
#include "Logger/ChainLogger.h"
#include "Logger/ConsoleLogger.h"
#include "Logger/DebugLogger.h"
#include "Utility.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API ILogger;
}

namespace KxVFS
{
	class KxVFS_API FileSystemService
	{
		public:
			static FileSystemService* GetInstance();

			static DynamicStringW GetLibraryVersion();
			static DynamicStringW GetDokanyVersion();

			static DynamicStringW GetDokanyDefaultServiceName();
			static DynamicStringW GetDokanyDefaultDriverPath();
			static bool IsDokanyDefaultInstallPresent();

		public:
			using TActiveFileSystems = std::list<IFileSystem*>;

		private:
			TActiveFileSystems m_ActiveFileSystems;

			mutable ServiceManager m_ServiceManager;
			DynamicStringW m_ServiceName;

			const bool m_HasSeSecurityNamePrivilege = false;
			bool m_IsFSInitialized = false;

			ChainLogger m_ChainLogger;
			StdOutLogger m_StdOutLogger;
			DebugLogger m_DebugLogger;

		private:
			ServiceHandle OpenService(ServiceAccess serviceAccess) const
			{
				return OpenService(m_ServiceManager, serviceAccess);
			}
			ServiceHandle OpenService(ServiceManager& manager, ServiceAccess serviceAccess) const;

			bool AddSeSecurityNamePrivilege();
			bool InitDriver();

		public:
			FileSystemService(DynamicStringRefW serviceName = {});
			virtual ~FileSystemService();

		public:
			bool IsOK() const;
			bool InitService(DynamicStringRefW name);
			
			DynamicStringRefW GetServiceName() const;
			bool IsInstalled() const;
			bool IsStarted() const;
			bool HasSeSecurityNamePrivilege() const
			{
				return m_HasSeSecurityNamePrivilege;
			}

			bool Start();
			bool Stop();
			bool Install(DynamicStringRefW binaryPath, DynamicStringRefW displayName = {}, DynamicStringRefW description = {});
			bool Uninstall();
			
			bool IsUsingDefaultDokanyInstallation() const;
			bool UseDefaultDokanyInstallation();

			TActiveFileSystems& GetActiveFileSystems()
			{
				return m_ActiveFileSystems;
			}
			void AddActiveFS(IFileSystem& fileSystem);
			void RemoveActiveFS(IFileSystem& fileSystem);

		public:
			virtual ILogger& GetLogger()
			{
				return m_ChainLogger;
			}
	};
}
