/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeDokan.h"
#include "KxVFSIDispatcher.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxDynamicString.h"
#include "Utility/KxVFSCriticalSection.h"

class KxVFSService;
class KxVFS_API KxVFSBase: public KxVFSIDispatcher
{
	public:
		static bool UnMountDirectory(const WCHAR* mountPoint);
		static bool IsCodeSuccess(int errorCode);
		static KxDynamicString GetErrorCodeMessage(int errorCode);

		// Writes a string 'source' into specified buffer but no more than 'maxDestLength' CHARS.
		// Returns number of BYTES written.
		static size_t WriteString(const WCHAR* source, WCHAR* destination, size_t maxDestLength);

	private:
		KxVFSService* m_ServiceInstance = NULL;
		DOKAN_OPTIONS m_Options = {0};
		DOKAN_OPERATIONS m_Operations = {0};
		DOKAN_HANDLE m_Handle = NULL;

		std::wstring m_MountPoint;
		bool m_IsMounted = false;
		KxVFSCriticalSection m_UnmountCS;

	private:
		void SetMounted(bool value);
		int DoMount();
		bool DoUnMount();

	public:
		static const size_t MaxPathLength = 32768;
		static const ULONG DefFlags = 0;
		static const ULONG DefRequestTimeout = 0;

		KxVFSBase(KxVFSService* vfsService, const WCHAR* mountPoint, ULONG falgs = DefFlags, ULONG requestTimeout = DefRequestTimeout);
		virtual ~KxVFSBase();

	public:
		virtual int Mount();
		virtual bool UnMount();

		virtual const WCHAR* GetVolumeName() const;
		virtual const WCHAR* GetVolumeFileSystemName() const;
		virtual ULONG GetVolumeSerialNumber() const;

		KxVFSService* GetService();
		const KxVFSService* GetService() const;
		bool IsMounted() const;

		const WCHAR* GetMountPoint() const
		{
			return m_MountPoint.c_str();
		}
		KxDynamicStringRef GetMountPointRef() const
		{
			return KxDynamicStringRef(m_MountPoint);
		}
		bool SetMountPoint(const WCHAR* mountPoint);
		
		ULONG GetFlags() const;
		bool SetFlags(ULONG falgs);

		NTSTATUS GetNtStatusByWin32ErrorCode(DWORD nWin32ErrorCode) const;
		NTSTATUS GetNtStatusByWin32LastErrorCode() const;

	private:
		NTSTATUS OnMountInternal(DOKAN_MOUNTED_INFO* eventInfo);
		NTSTATUS OnUnMountInternal(DOKAN_UNMOUNTED_INFO* eventInfo);

	protected:
		// For OnMount, OnUnMount, OnCloseFile, OnCleanUp return 'STATUS_NOT_SUPPORTED'
		// as underlaying function has void as its return type.
		virtual NTSTATUS OnMount(DOKAN_MOUNTED_INFO* eventInfo) = 0;
		virtual NTSTATUS OnUnMount(DOKAN_UNMOUNTED_INFO* eventInfo) = 0;

		virtual NTSTATUS OnGetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnGetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnGetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* eventInfo) = 0;

		virtual NTSTATUS OnCreateFile(DOKAN_CREATE_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnCloseFile(DOKAN_CLOSE_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnCleanUp(DOKAN_CLEANUP_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnMoveFile(DOKAN_MOVE_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* eventInfo) = 0;

		virtual NTSTATUS OnLockFile(DOKAN_LOCK_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnUnlockFile(DOKAN_UNLOCK_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* eventInfo) = 0;

		virtual NTSTATUS OnReadFile(DOKAN_READ_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnWriteFile(DOKAN_WRITE_FILE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnFlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnSetEndOfFile(DOKAN_SET_EOF_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnSetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnGetFileInfo(DOKAN_GET_FILE_INFO_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnSetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* eventInfo) = 0;

		virtual NTSTATUS OnFindFiles(DOKAN_FIND_FILES_EVENT* eventInfo) = 0;
		virtual NTSTATUS OnFindStreams(DOKAN_FIND_STREAMS_EVENT* eventInfo) = 0;

	private:
		inline static KxVFSBase* GetFromContext(DOKAN_OPTIONS* pDokanOptions)
		{
			return reinterpret_cast<KxVFSBase*>(pDokanOptions->GlobalContext);
		}
		template<class EventInfoT> inline static KxVFSBase* GetFromContext(const EventInfoT* eventInfo)
		{
			return GetFromContext(eventInfo->DokanFileInfo->DokanOptions);
		}

		static void DOKAN_CALLBACK Dokan_Mount(DOKAN_MOUNTED_INFO* eventInfo);
		static void DOKAN_CALLBACK Dokan_Unmount(DOKAN_UNMOUNTED_INFO* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_CreateFile(DOKAN_CREATE_FILE_EVENT* eventInfo);
		static void DOKAN_CALLBACK Dokan_CloseFile(DOKAN_CLOSE_FILE_EVENT* eventInfo);
		static void DOKAN_CALLBACK Dokan_CleanUp(DOKAN_CLEANUP_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_MoveFile(DOKAN_MOVE_FILE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_CanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_LockFile(DOKAN_LOCK_FILE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_UnlockFile(DOKAN_UNLOCK_FILE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_ReadFile(DOKAN_READ_FILE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_WriteFile(DOKAN_WRITE_FILE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_FlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetEndOfFile(DOKAN_SET_EOF_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetFileInfo(DOKAN_GET_FILE_INFO_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_FindFiles(DOKAN_FIND_FILES_EVENT* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_FindStreams(DOKAN_FIND_STREAMS_EVENT* eventInfo);
};
