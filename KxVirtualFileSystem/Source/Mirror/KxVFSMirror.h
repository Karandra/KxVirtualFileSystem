#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSIDispatcher.h"
#include "Utility/KxVFSUtility.h"
class KxVFSMirror_FileHandle;
class KxVFSMirror_Overlapped;
enum KxVFSMirror_IOOperationType;

class KxVFSService;
class KxVFS_API KxVFSMirror: public KxVFSBase
{
	private:
		std::wstring m_Source;

	private:
		volatile LONG m_IsUnmounted = 0;
		DOKAN_VECTOR m_FileHandlePool = {0};
		KxVFSCriticalSection m_FileHandlePoolCS;

		#if KxVFS_USE_ASYNC_IO
		TP_CALLBACK_ENVIRON m_ThreadPoolEnvironment = {0};
		PTP_CLEANUP_GROUP m_ThreadPoolCleanupGroup = NULL;
		PTP_POOL m_ThreadPool = NULL;
		KxVFSCriticalSection m_ThreadPoolCS;
		DOKAN_VECTOR m_OverlappedPool = {0};
		KxVFSCriticalSection m_OverlappedPoolCS;
		#endif

	protected:
		DWORD GetParentSecurity(const WCHAR* sFilePath, PSECURITY_DESCRIPTOR* pParentSecurity) const;
		DWORD CreateNewSecurity(DOKAN_CREATE_FILE_EVENT* pEventInfo, const WCHAR* sFilePath, PSECURITY_DESCRIPTOR pRequestedSecurity, PSECURITY_DESCRIPTOR* pNewSecurity) const;

		bool CheckDeleteOnClose(PDOKAN_FILE_INFO pFileInfo, const WCHAR* sFilePath) const;
		NTSTATUS CanDeleteDirectory(const WCHAR* sFilePath) const;
		NTSTATUS ReadFileSynchronous(DOKAN_READ_FILE_EVENT* pEventInfo, HANDLE hFileHandle) const;
		NTSTATUS WriteFileSynchronous(DOKAN_WRITE_FILE_EVENT* pEventInfo, HANDLE hFileHandle, UINT64 nFileSize) const;

	public:
		virtual KxDynamicString GetTargetPath(const WCHAR* sRequestedPath) override;

	public:
		KxVFSMirror(KxVFSService* pVFSService, const WCHAR* sMountPoint, const WCHAR* sSource, ULONG nFalgs = DefFlags, ULONG nRequestTimeout = DefRequestTimeout);
		virtual ~KxVFSMirror();

	public:
		const WCHAR* GetSource() const
		{
			return m_Source.c_str();
		}
		KxDynamicStringRef GetSourceRef() const
		{
			return KxDynamicStringRef(m_Source);
		}
		bool SetSource(const WCHAR* sSource);

		virtual int Mount() override;

	protected:
		virtual NTSTATUS OnMount(DOKAN_MOUNTED_INFO* pEventInfo) override;
		virtual NTSTATUS OnUnMount(DOKAN_UNMOUNTED_INFO* pEventInfo) override;

		virtual NTSTATUS OnGetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnGetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* pEventInfo) override;
		virtual NTSTATUS OnGetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* pEventInfo) override;

		virtual NTSTATUS OnCreateFile(DOKAN_CREATE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCloseFile(DOKAN_CLOSE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCleanUp(DOKAN_CLEANUP_EVENT* pEventInfo) override;
		virtual NTSTATUS OnMoveFile(DOKAN_MOVE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* pEventInfo) override;

		virtual NTSTATUS OnLockFile(DOKAN_LOCK_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnUnlockFile(DOKAN_UNLOCK_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* pEventInfo) override;
		virtual NTSTATUS OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* pEventInfo) override;

		virtual NTSTATUS OnReadFile(DOKAN_READ_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnWriteFile(DOKAN_WRITE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnFlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* pEventInfo) override;
		virtual NTSTATUS OnSetEndOfFile(DOKAN_SET_EOF_EVENT* pEventInfo) override;
		virtual NTSTATUS OnSetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnGetFileInfo(DOKAN_GET_FILE_INFO_EVENT* pEventInfo) override;
		virtual NTSTATUS OnSetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* pEventInfo) override;

		virtual NTSTATUS OnFindFiles(DOKAN_FIND_FILES_EVENT* pEventInfo) override;
		virtual NTSTATUS OnFindStreams(DOKAN_FIND_STREAMS_EVENT* pEventInfo) override;

	protected:
		void FreeMirrorFileHandle(KxVFSMirror_FileHandle* FileHandle);
		void PushMirrorFileHandle(KxVFSMirror_FileHandle* FileHandle);
		KxVFSMirror_FileHandle* PopMirrorFileHandle(HANDLE ActualFileHandle);
		void GetMirrorFileHandleState(KxVFSMirror_FileHandle* pFileHandle, bool* pIsCleanedUp, bool* bIsClosed) const;

		BOOL InitializeMirrorFileHandles();
		void CleanupMirrorFileHandles();

		#if KxVFS_USE_ASYNC_IO
		void FreeMirrorOverlapped(KxVFSMirror_Overlapped* Overlapped) const;
		void PushMirrorOverlapped(KxVFSMirror_Overlapped* Overlapped);
		KxVFSMirror_Overlapped* PopMirrorOverlapped();

		BOOL InitializeAsyncIO();
		void CleanupPendingAsyncIO();
		void CleanupAsyncIO();
		static void CALLBACK MirrorIoCallback
		(
			PTP_CALLBACK_INSTANCE Instance,
			PVOID Context,
			PVOID Overlapped,
			ULONG IoResult,
			ULONG_PTR NumberOfBytesTransferred,
			PTP_IO Io
		);
		#endif
};
