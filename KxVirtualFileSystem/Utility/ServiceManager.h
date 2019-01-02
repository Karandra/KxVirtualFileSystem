/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "IncludeWindows.h"

namespace KxVFS
{
	class ServiceManager
	{
		private:
			SC_HANDLE m_Handle = nullptr;
			uint32_t m_LastError = 0;

		public:
			ServiceManager(uint32_t accessMode = SERVICE_ALL_ACCESS)
			{
				m_Handle = ::OpenSCManagerW(nullptr, nullptr, accessMode);
				m_LastError = ::GetLastError();
			}
			~ServiceManager()
			{
				::CloseServiceHandle(m_Handle);
			}

		public:
			bool IsOK() const
			{
				return m_Handle != nullptr;
			}
			operator SC_HANDLE() const
			{
				return m_Handle;
			}
	};
}
