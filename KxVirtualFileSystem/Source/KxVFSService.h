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
		static const WCHAR* GetDokanVersion();

	private:
		static DWORD GetServiceStatus(SC_HANDLE hService);
		static SC_HANDLE CreateService(SC_HANDLE hServiceManager, const WCHAR* sDriverPath, const WCHAR* sServiceName, const WCHAR* sDisplayName, const WCHAR* sDescription);
		static bool ReconfigureService(SC_HANDLE hService, const WCHAR* sDriverPath, const WCHAR* sDisplayName, const WCHAR* sDescription);
		static bool SetServiceDescription(SC_HANDLE hService, const WCHAR* sDescription);

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
		void AddVFS(KxVFSBase* pVFS);
		void RemoveVFS(KxVFSBase* pVFS);

		bool AddSeSecurityNamePrivilege();

	public:
		KxVFSService(const WCHAR* sServiceName);
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
		bool Install(const WCHAR* sDriverPath, const WCHAR* sDisplayName = NULL, const WCHAR* sDescription = NULL);
		bool Uninstall();
};
