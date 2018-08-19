#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility/KxVFSCriticalSection.h"
#include "Utility/KxVFSContext.h"
class KxVFSMirror;

class KxVFSMirror_FileHandle
{
	public:
		HANDLE FileHandle = NULL;
		#if KxVFS_USE_ASYNC_IO
		PTP_IO IoCompletion = NULL;
		#endif

		KxVFSCriticalSection Lock;
		bool IsCleanedUp = false;
		bool IsClosed = false;
		KxVFSMirror* self = NULL;

	public:
		KxVFSMirror_FileHandle(KxVFSMirror* mirror)
			:self(mirror)
		{
		}
};

enum KxVFSMirror_IOOperationType
{
	MIRROR_IOTYPE_UNKNOWN = 0,
	MIRROR_IOTYPE_READ,
	MIRROR_IOTYPE_WRITE
};
class KxVFSMirror_Overlapped
{
	public:
		OVERLAPPED InternalOverlapped = {0};
		KxVFSMirror_FileHandle* FileHandle = NULL;
		KxVFSMirror_IOOperationType IoType = MIRROR_IOTYPE_UNKNOWN;
		void* Context = NULL;
};
