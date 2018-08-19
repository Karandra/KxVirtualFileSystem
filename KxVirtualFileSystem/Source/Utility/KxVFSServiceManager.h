#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeWindows.h"

class KxVFSServiceManager
{
	private:
		SC_HANDLE m_Handle = NULL;
		DWORD m_LastError = 0;

	public:
		KxVFSServiceManager(DWORD accessMode = SERVICE_ALL_ACCESS)
		{
			m_Handle = ::OpenSCManagerW(NULL, NULL, accessMode);
			m_LastError = ::GetLastError();
		}
		~KxVFSServiceManager()
		{
			::CloseServiceHandle(m_Handle);
		}

	public:
		bool IsOK() const
		{
			return m_Handle != NULL;
		}
		operator SC_HANDLE() const
		{
			return m_Handle;
		}
};
