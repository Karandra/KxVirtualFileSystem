#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"

namespace KxVFS
{
	enum class KernelFileOptions: uint32_t
	{
		None = 0,

		Supersede = FILE_SUPERSEDE,
		Open = FILE_OPEN,
		Create = FILE_CREATE,
		OpenIf = FILE_OPEN_IF,
		Overwrite = FILE_OVERWRITE,
		OverwriteIf = FILE_OVERWRITE_IF,
		MaximumDisposition = FILE_MAXIMUM_DISPOSITION,

		DirectoryFile = FILE_DIRECTORY_FILE,
		WriteThrough = FILE_WRITE_THROUGH,
		SequentialOnly = FILE_SEQUENTIAL_ONLY,
		NoIntermediateBuffering = FILE_NO_INTERMEDIATE_BUFFERING,

		SynchronousIOAlert = FILE_SYNCHRONOUS_IO_ALERT,
		SynchronousIONonAlert = FILE_SYNCHRONOUS_IO_NONALERT,
		NonDirectoryFile = FILE_NON_DIRECTORY_FILE,
		CreateTreeConnection = FILE_CREATE_TREE_CONNECTION,

		CompleteIfOplocked = FILE_COMPLETE_IF_OPLOCKED,
		NoEAKnowledge = FILE_NO_EA_KNOWLEDGE,
		OpenRemoteInstance = FILE_OPEN_REMOTE_INSTANCE,
		RandomAccess = FILE_RANDOM_ACCESS,

		DeleteOnClose = FILE_DELETE_ON_CLOSE,
		OpenByFileID = FILE_OPEN_BY_FILE_ID,
		OpenForBackupIntent = FILE_OPEN_FOR_BACKUP_INTENT,
		NoCompression = FILE_NO_COMPRESSION,

		OpenRequiringOplock = FILE_OPEN_REQUIRING_OPLOCK,
		DisallowExclusive = FILE_DISALLOW_EXCLUSIVE,
		SessionAware = FILE_SESSION_AWARE,

		ReserveOpFilter = FILE_RESERVE_OPFILTER,
		OpenReparsePoint = FILE_OPEN_REPARSE_POINT,
		OpenNoRecall = FILE_OPEN_NO_RECALL,
		OpenForFreeSpaceQuery = FILE_OPEN_FOR_FREE_SPACE_QUERY,

		ValidOptionFlags = FILE_VALID_OPTION_FLAGS,

		Superseded = FILE_SUPERSEDED,
		Opened = FILE_OPENED,
		Created = FILE_CREATED,
		Overwritten = FILE_OVERWRITTEN,
		Exists = FILE_EXISTS,
		DoesNotExist = FILE_DOES_NOT_EXIST,

		WriteToEndOfFile = FILE_WRITE_TO_END_OF_FILE,
		UseFilePointerPosition = FILE_USE_FILE_POINTER_POSITION,
	};
	KxVFS_DeclareFlagSet(KernelFileOptions);
}
