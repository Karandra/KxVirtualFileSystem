/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility.h"

namespace KxVFS
{
	class MirrorFS;
}

namespace KxVFS::Mirror
{
	#if !KxVFS_USE_ASYNC_IO
	using PTP_IO = const void*;
	#endif

	class FileContext
	{
		public:
			MirrorFS* m_VFSInstance = nullptr;
			
			CriticalSection m_Lock;
			HANDLE m_FileHandle = INVALID_HANDLE_VALUE;
			PTP_IO m_IOCompletion = nullptr;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(MirrorFS* mirror)
				:m_VFSInstance(mirror)
			{
			}
	};

	enum class IOOperationType: int
	{
		Unknown = 0,
		Read,
		Write
	};
	class OverlappedContext
	{
		public:
			OVERLAPPED m_InternalOverlapped = {0};
			FileContext* m_FileHandle = nullptr;
			IOOperationType m_IOType = IOOperationType::Unknown;
			void* m_Context = nullptr;
	};
}
