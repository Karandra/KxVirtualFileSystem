#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/IncludeWindows.h"
#include "HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API GenericHandle: public HandleWrapper<GenericHandle, HANDLE, size_t, std::numeric_limits<size_t>::max()>
	{
		friend class TWrapper;

		protected:
			static void DoCloseHandle(THandle handle)
			{
				::CloseHandle(handle);
			}

		public:
			GenericHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}
	};
}
