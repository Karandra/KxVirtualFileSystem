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
class KxVFS_API KxVFSBase: public KxVFSEvents, public KxVFSIDispatcher
{
	public:
		static bool UnMountDirectory(const WCHAR* mountPoint);
		static bool IsCodeSuccess(int errorCode);
		static KxDynamicString GetErrorCodeMessage(int errorCode);

		static constexpr bool IsUnsingAsyncIO()
		{
			return KxVFS_USE_ASYNC_IO;
		}

		// Writes a string 'source' into specified buffer but no more than 'maxDestLength' CHARS.
		// Returns number of BYTES written.
		static size_t WriteString(const WCHAR* source, WCHAR* destination, size_t maxDestLength);

	private:
		KxVFSService* m_ServiceInstance = NULL;
		Dokany2::DOKAN_OPTIONS m_Options = {0};
		Dokany2::DOKAN_OPERATIONS m_Operations = {0};
		Dokany2::DOKAN_HANDLE m_Handle = NULL;

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
		NTSTATUS OnMountInternal(EvtMounted& eventInfo);
		NTSTATUS OnUnMountInternal(EvtUnMounted& eventInfo);

	protected:
		// For OnMount, OnUnMount, OnCloseFile, OnCleanUp return 'STATUS_NOT_SUPPORTED'
		// as underlaying function has void as its return type.
		virtual NTSTATUS OnMount(EvtMounted& eventInfo) = 0;
		virtual NTSTATUS OnUnMount(EvtUnMounted& eventInfo) = 0;

		virtual NTSTATUS OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) = 0;
		virtual NTSTATUS OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) = 0;
		virtual NTSTATUS OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) = 0;

		virtual NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) = 0;
		virtual NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) = 0;
		virtual NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) = 0;
		virtual NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) = 0;
		virtual NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) = 0;

		virtual NTSTATUS OnLockFile(EvtLockFile& eventInfo) = 0;
		virtual NTSTATUS OnUnlockFile(EvtUnlockFile& eventInfo) = 0;
		virtual NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) = 0;
		virtual NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) = 0;

		virtual NTSTATUS OnReadFile(EvtReadFile& eventInfo) = 0;
		virtual NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) = 0;
		virtual NTSTATUS OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) = 0;
		virtual NTSTATUS OnSetEndOfFile(EvtSetEndOfFile& eventInfo) = 0;
		virtual NTSTATUS OnSetAllocationSize(EvtSetAllocationSize& eventInfo) = 0;
		virtual NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) = 0;
		virtual NTSTATUS OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) = 0;

		virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) = 0;
		virtual NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) = 0;

	private:
		inline static KxVFSBase* GetFromContext(Dokany2::DOKAN_OPTIONS* pDokanOptions)
		{
			return reinterpret_cast<KxVFSBase*>(pDokanOptions->GlobalContext);
		}
		template<class T> inline static KxVFSBase* GetFromContext(const T* eventInfo)
		{
			return GetFromContext(eventInfo->DokanFileInfo->DokanOptions);
		}

		static void DOKAN_CALLBACK Dokan_Mount(EvtMounted* eventInfo);
		static void DOKAN_CALLBACK Dokan_Unmount(EvtUnMounted* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_CreateFile(EvtCreateFile* eventInfo);
		static void DOKAN_CALLBACK Dokan_CloseFile(EvtCloseFile* eventInfo);
		static void DOKAN_CALLBACK Dokan_CleanUp(EvtCleanUp* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_MoveFile(EvtMoveFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_LockFile(EvtLockFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_UnlockFile(EvtUnlockFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_ReadFile(EvtReadFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_WriteFile(EvtWriteFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_GetFileInfo(EvtGetFileInfo* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo);

		static NTSTATUS DOKAN_CALLBACK Dokan_FindFiles(EvtFindFiles* eventInfo);
		static NTSTATUS DOKAN_CALLBACK Dokan_FindStreams(EvtFindStreams* eventInfo);
};
