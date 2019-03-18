#pragma once
/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

#pragma warning(disable: 4005) // Macro redefinition

extern "C"
{
	#pragma warning(push)
	#pragma warning(disable: 4505) // Unreferenced local function has been removed

	// Define _EXPORTING to link statically
	namespace Dokany2
	{
		#define _EXPORTING 1

		#include "Dokan/dokan.h"
		#include "Dokan/fileinfo.h"

		#undef _EXPORTING

	}
	#pragma warning(pop)
}

namespace KxVFS
{
	using EvtMounted = Dokany2::DOKAN_MOUNTED_INFO;
	using EvtUnMounted = Dokany2::DOKAN_UNMOUNTED_INFO;

	using EvtGetVolumeFreeSpace = Dokany2::DOKAN_GET_DISK_FREE_SPACE_EVENT;
	using EvtGetVolumeInfo = Dokany2::DOKAN_GET_VOLUME_INFO_EVENT;
	using EvtGetVolumeAttributes = Dokany2::DOKAN_GET_VOLUME_ATTRIBUTES_EVENT;

	using EvtCreateFile = Dokany2::DOKAN_CREATE_FILE_EVENT;
	using EvtCloseFile = Dokany2::DOKAN_CLOSE_FILE_EVENT;
	using EvtMoveFile = Dokany2::DOKAN_MOVE_FILE_EVENT;
	using EvtCleanUp = Dokany2::DOKAN_CLEANUP_EVENT;
	using EvtCanDeleteFile = Dokany2::DOKAN_CAN_DELETE_FILE_EVENT;

	using EvtLockFile = Dokany2::DOKAN_LOCK_FILE_EVENT;
	using EvtUnlockFile = Dokany2::DOKAN_UNLOCK_FILE_EVENT;
	using EvtGetFileSecurity = Dokany2::DOKAN_GET_FILE_SECURITY_EVENT;
	using EvtSetFileSecurity = Dokany2::DOKAN_SET_FILE_SECURITY_EVENT;

	using EvtReadFile = Dokany2::DOKAN_READ_FILE_EVENT;
	using EvtWriteFile = Dokany2::DOKAN_WRITE_FILE_EVENT;
	using EvtFlushFileBuffers = Dokany2::DOKAN_FLUSH_BUFFERS_EVENT;
	using EvtSetEndOfFile = Dokany2::DOKAN_SET_EOF_EVENT;
	using EvtSetAllocationSize = Dokany2::DOKAN_SET_ALLOCATION_SIZE_EVENT;
	using EvtGetFileInfo = Dokany2::DOKAN_GET_FILE_INFO_EVENT;
	using EvtSetBasicFileInfo = Dokany2::DOKAN_SET_FILE_BASIC_INFO_EVENT;

	using EvtFindFiles = Dokany2::DOKAN_FIND_FILES_EVENT;
	using EvtFindStreams = Dokany2::DOKAN_FIND_STREAMS_EVENT;
}
