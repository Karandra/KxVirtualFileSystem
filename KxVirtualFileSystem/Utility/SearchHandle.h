/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/HandleWrapper.h"

namespace KxVFS::Utility
{
	class KxVFS_API SearchHandle: public HandleWrapper<SearchHandle, size_t, std::numeric_limits<size_t>::max()>
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
