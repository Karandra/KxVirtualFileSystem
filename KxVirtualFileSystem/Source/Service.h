/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "IncludeWindows.h"
#include "Utility.h"

namespace KxVFS
{
	class AbstractFS;

	class KxVFS_API Service
	{
		friend class AbstractFS;

		public:
			static KxDynamicStringW GetLibraryVersion();
			static KxDynamicStringW GetDokanyVersion();

		private:
			static DWORD GetServiceStatus(SC_HANDLE serviceHandle);
			static SC_HANDLE CreateService(SC_HANDLE serviceManagerHandle, KxDynamicStringRefW driverPath, KxDynamicStringRefW serviceName, KxDynamicStringRefW displayName, KxDynamicStringRefW description);
			static bool ReconfigureService(SC_HANDLE serviceHandle, KxDynamicStringRefW driverPath, KxDynamicStringRefW displayName, KxDynamicStringRefW description);
			static bool SetServiceDescription(SC_HANDLE serviceHandle, KxDynamicStringRefW description);

		public:
			static const WORD DefualtThreadsCount = 4;
			static const DWORD ServiceStartType = SERVICE_DEMAND_START;
			typedef std::list<AbstractFS*> VFSListType;

		private:
			KxDynamicStringW m_ServiceName;
			ServiceManager m_ServiceManager;
			SC_HANDLE m_ServiceHandle = nullptr;
			VFSListType m_VFSList;
			bool m_HasSeSecurityNamePrivilege = false;

		private:
			VFSListType& GetVFSList()
			{
				return m_VFSList;
			}
			void AddFS(AbstractFS* vfs);
			void RemoveFS(AbstractFS* vfs);

			bool AddSeSecurityNamePrivilege();

		public:
			Service(KxDynamicStringRefW serviceName);
			virtual ~Service();

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
			bool Install(KxDynamicStringRefW driverPath, KxDynamicStringRefW displayName = {}, KxDynamicStringRefW description = {});
			bool Uninstall();
	};
}
