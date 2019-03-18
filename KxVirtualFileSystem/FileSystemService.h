/*
Copyright © 2018 Kerber. All rights reserved.

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

		public:
			typedef std::list<IFileSystem*> TActiveFileSystems;

		private:
			TActiveFileSystems m_ActiveFileSystems;

			KxDynamicStringW m_ServiceName;
			ServiceManager m_ServiceManager;
			ServiceHandle m_DriverService;
			bool m_HasSeSecurityNamePrivilege = false;

		private:
			bool AddSeSecurityNamePrivilege();

		public:
			FileSystemService(KxDynamicStringRefW serviceName);
			virtual ~FileSystemService();

		public:
			bool IsOK() const;
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

			TActiveFileSystems& GetActiveFileSystems()
			{
				return m_ActiveFileSystems;
			}
			void AddActiveFS(IFileSystem& fileSystem);
			void RemoveActiveFS(IFileSystem& fileSystem);
	};
}
