#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API SearchHandle: public HandleWrapper<SearchHandle, HANDLE, size_t, std::numeric_limits<size_t>::max()>
	{
		friend class TWrapper;

		protected:
			static void DoCloseHandle(THandle handle)
			{
				::FindClose(handle);
			}

		public:
			SearchHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}
	};
}
