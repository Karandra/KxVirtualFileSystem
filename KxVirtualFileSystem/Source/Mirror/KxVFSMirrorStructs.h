/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility/KxVFSCriticalSection.h"
#include "Utility/KxVFSContext.h"
class KxVFSMirror;

namespace KxVFSMirrorNS
{
	class FileHandle
	{
		public:
			HANDLE m_FileHandle = NULL;
			#if KxVFS_USE_ASYNC_IO
			PTP_IO m_IOCompletion = NULL;
			#endif

			KxVFSCriticalSection m_Lock;
			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;
			KxVFSMirror* m_MirrorInstance = NULL;

		public:
			FileHandle(KxVFSMirror* mirror)
				:m_MirrorInstance(mirror)
			{
			}
	};

	enum IOOperationType: int
	{
		MIRROR_IOTYPE_UNKNOWN = 0,
		MIRROR_IOTYPE_READ,
		MIRROR_IOTYPE_WRITE
	};
	class Overlapped
	{
		public:
			OVERLAPPED m_InternalOverlapped = {0};
			FileHandle* m_FileHandle = NULL;
			IOOperationType m_IOType = MIRROR_IOTYPE_UNKNOWN;
			void* m_Context = NULL;
	};

}
