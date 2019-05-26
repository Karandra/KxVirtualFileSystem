/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Misc/IncludeWindows.h"
#include "Utility.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
}

namespace KxVFS
{
	class KxVFS_API FileSystemService
	{
		public:
			static KxDynamicStringW GetLibraryVersion();
			static KxDynamicStringW GetDokanyVersion();

			static KxDynamicStringW GetDokanyDefaultServiceName();
			static KxDynamicStringW GetDokanyDefaultDriverPath();
			static bool IsDokanyDefaultInstallPresent();

		public:
			using TActiveFileSystems = std::list<IFileSystem*>;

		private:
			TActiveFileSystems m_ActiveFileSystems;

			KxDynamicStringW m_ServiceName;
			ServiceManager m_ServiceManager;
			ServiceHandle m_DriverService;
			const bool m_HasSeSecurityNamePrivilege = false;
			bool m_IsFSInitialized = false;

		private:
			bool AddSeSecurityNamePrivilege();
			bool InitDriver();

		public:
			FileSystemService(KxDynamicStringRefW serviceName = {});
			virtual ~FileSystemService();

		public:
			bool IsOK() const;
			const ServiceHandle& GetDriverService() const
			{
				return m_DriverService;
			}
			bool InitService(KxDynamicStringRefW name);
			
			KxDynamicStringRefW GetServiceName() const;
			bool IsInstalled() const;
			bool IsStarted() const;
			bool HasSeSecurityNamePrivilege() const
			{
				return m_HasSeSecurityNamePrivilege;
			}

			bool Start();
			bool Stop();
			bool Install(KxDynamicStringRefW binaryPath, KxDynamicStringRefW displayName = {}, KxDynamicStringRefW description = {});
			bool Uninstall();
			
			bool IsUsingDefaultDokanyInstallation() const;
			bool UseDefaultDokanyInstallation();

			TActiveFileSystems& GetActiveFileSystems()
			{
				return m_ActiveFileSystems;
			}
			void AddActiveFS(IFileSystem& fileSystem);
			void RemoveActiveFS(IFileSystem& fileSystem);
	};
}
