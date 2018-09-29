/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSMirrorStructs.h"
#include "KxVFSIDispatcher.h"
#include "Utility/KxVFSUtility.h"

class KxVFSService;
class KxVFS_API KxVFSMirror: public KxVFSBase
{
	private:
		std::wstring m_Source;

	private:
		volatile LONG m_IsUnmounted = 0;
		Dokany2::DOKAN_VECTOR m_FileHandlePool = {0};
		KxVFSCriticalSection m_FileHandlePoolCS;

		#if KxVFS_USE_ASYNC_IO
		TP_CALLBACK_ENVIRON m_ThreadPoolEnvironment = {0};
		PTP_CLEANUP_GROUP m_ThreadPoolCleanupGroup = NULL;
		PTP_POOL m_ThreadPool = NULL;
		KxVFSCriticalSection m_ThreadPoolCS;
		Dokany2::DOKAN_VECTOR m_OverlappedPool = {0};
		KxVFSCriticalSection m_OverlappedPoolCS;
		#endif

	protected:
		DWORD GetParentSecurity(const WCHAR* filePath, PSECURITY_DESCRIPTOR* parentSecurity) const;
		DWORD CreateNewSecurity(EvtCreateFile& eventInfo, const WCHAR* filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const;

		bool CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, const WCHAR* filePath) const;
		NTSTATUS CanDeleteDirectory(const WCHAR* filePath) const;
		NTSTATUS ReadFileSynchronous(EvtReadFile& eventInfo, HANDLE fileHandle) const;
		NTSTATUS WriteFileSynchronous(EvtWriteFile& eventInfo, HANDLE fileHandle, UINT64 fileSize) const;

	public:
		virtual KxDynamicString GetTargetPath(const WCHAR* requestedPath) override;

	public:
		KxVFSMirror(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* source, ULONG falgs = DefFlags, ULONG requestTimeout = DefRequestTimeout);
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
		bool SetSource(const WCHAR* source);

		virtual int Mount() override;

	protected:
		virtual NTSTATUS OnMount(EvtMounted& eventInfo) override;
		virtual NTSTATUS OnUnMount(EvtUnMounted& eventInfo) override;

		virtual NTSTATUS OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) override;
		virtual NTSTATUS OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) override;
		virtual NTSTATUS OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) override;

		virtual NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
		virtual NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) override;
		virtual NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) override;
		virtual NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;
		virtual NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;

		virtual NTSTATUS OnLockFile(EvtLockFile& eventInfo) override;
		virtual NTSTATUS OnUnlockFile(EvtUnlockFile& eventInfo) override;
		virtual NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) override;
		virtual NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) override;

		virtual NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
		virtual NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;
		virtual NTSTATUS OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) override;
		virtual NTSTATUS OnSetEndOfFile(EvtSetEndOfFile& eventInfo) override;
		virtual NTSTATUS OnSetAllocationSize(EvtSetAllocationSize& eventInfo) override;
		virtual NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) override;
		virtual NTSTATUS OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) override;

		virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
		virtual NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) override;

	protected:
		void FreeMirrorFileHandle(KxVFSMirrorNS::FileHandle* fileHandle);
		void PushMirrorFileHandle(KxVFSMirrorNS::FileHandle* fileHandle);
		KxVFSMirrorNS::FileHandle* PopMirrorFileHandle(HANDLE actualFileHandle);
		void GetMirrorFileHandleState(KxVFSMirrorNS::FileHandle* fileHandle, bool* isCleanedUp, bool* isClosed) const;

		BOOL InitializeMirrorFileHandles();
		void CleanupMirrorFileHandles();

		#if KxVFS_USE_ASYNC_IO
		void FreeMirrorOverlapped(KxVFSMirrorNS::Overlapped* overlappedBuffer) const;
		void PushMirrorOverlapped(KxVFSMirrorNS::Overlapped* overlappedBuffer);
		KxVFSMirrorNS::Overlapped* PopMirrorOverlapped();

		BOOL InitializeAsyncIO();
		void CleanupPendingAsyncIO();
		void CleanupAsyncIO();
		static void CALLBACK MirrorIoCallback
		(
			PTP_CALLBACK_INSTANCE instance,
			PVOID context,
			PVOID overlappedBuffer,
			ULONG resultIO,
			ULONG_PTR numberOfBytesTransferred,
			PTP_IO portIO
		);
		#endif
};
