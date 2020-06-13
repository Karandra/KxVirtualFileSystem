#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/IncludeWindows.h"
#include "HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API TokenHandle: public HandleWrapper<TokenHandle, HANDLE, size_t, std::numeric_limits<size_t>::max()>
	{
		friend class TWrapper;

		protected:
			static void DoCloseHandle(THandle handle) noexcept
			{
				::CloseHandle(handle);
			}

		public:
			TokenHandle(THandle fileHandle = GetInvalidHandle()) noexcept
				:HandleWrapper(fileHandle)
			{
			}
	};
}
