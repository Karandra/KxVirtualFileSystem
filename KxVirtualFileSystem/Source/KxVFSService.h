/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeWindows.h"
#include "Utility/KxVFSServiceManager.h"

class KxVFSBase;
class KxVFSConvergence;
class KxVFS_API KxVFSService
{
	friend class KxVFSBase;
	friend class KxVFSConvergence;

	public:
		static const WCHAR* GetLibraryVersion();
		static const WCHAR* GetDokanyVersion();

	private:
		static DWORD GetServiceStatus(SC_HANDLE serviceHandle);
		static SC_HANDLE CreateService(SC_HANDLE serviceManagerHandle, const WCHAR* driverPath, const WCHAR* serviceName, const WCHAR* displayName, const WCHAR* description);
		static bool ReconfigureService(SC_HANDLE serviceHandle, const WCHAR* driverPath, const WCHAR* displayName, const WCHAR* description);
		static bool SetServiceDescription(SC_HANDLE serviceHandle, const WCHAR* description);

	public:
		static const WORD DefualtThreadsCount = 4;
		static const DWORD ServiceStartType = SERVICE_DEMAND_START;
		typedef std::list<KxVFSBase*> VFSListType;

	private:
		std::wstring m_ServiceName;
		KxVFSServiceManager m_ServiceManager;
		SC_HANDLE m_ServiceHandle = NULL;
		VFSListType m_VFSList;
		bool m_HasSeSecurityNamePrivilege = false;

	private:
		VFSListType& GetVFSList()
		{
			return m_VFSList;
		}
		void AddVFS(KxVFSBase* vfs);
		void RemoveVFS(KxVFSBase* vfs);

		bool AddSeSecurityNamePrivilege();

	public:
		KxVFSService(const WCHAR* serviceName);
		virtual ~KxVFSService();

	public:
		bool IsOK() const;
		const WCHAR* GetServiceName() const;
		bool IsInstalled() const;
		bool IsStarted() const;
		bool HasSeSecurityNamePrivilege() const
		{
			return m_HasSeSecurityNamePrivilege;
		}

		bool Start();
		bool Stop();
		bool Install(const WCHAR* driverPath, const WCHAR* displayName = NULL, const WCHAR* description = NULL);
		bool Uninstall();
};
