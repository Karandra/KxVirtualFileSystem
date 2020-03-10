/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "ServiceConstants.h"
#include "ServiceHandle.h"

namespace KxVFS
{
	class KxVFS_API ServiceManager final
	{
		private:
			ServiceHandle m_Handle;

		public:
			ServiceManager() = default;
			ServiceManager(ServiceManagerAccess accessMode)
			{
				Open(accessMode);
			}

		public:
			bool IsOK() const
			{
				return m_Handle.IsValid();
			}
			bool Open(ServiceManagerAccess accessMode);

		public:
			operator SC_HANDLE() const
			{
				return m_Handle;
			}

			explicit operator bool() const noexcept
			{
				return IsOK();
			}
			bool operator!() const noexcept
			{
				return !IsOK();
			}
	};
}
