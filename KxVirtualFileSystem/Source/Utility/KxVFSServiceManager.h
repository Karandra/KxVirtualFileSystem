/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
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
