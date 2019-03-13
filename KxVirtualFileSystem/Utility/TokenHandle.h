/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API TokenHandle: public HandleWrapper<TokenHandle, size_t, std::numeric_limits<size_t>::max()>
	{
		friend class TWrapper;

		protected:
			static void DoCloseHandle(THandle handle)
			{
				::CloseHandle(handle);
			}

		public:
			TokenHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}
	};
}
