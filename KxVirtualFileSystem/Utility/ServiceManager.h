#pragma once
#include "KxVirtualFileSystem/Common.hpp"
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
