/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility.h"
#include "Misc/IncludeDokan.h"
#include "Common/FSError.h"
#include "Common/FSFlags.h"
#include "Common/FileNode.h"
#include "Common/IOManager.h"
#include "Common/FileContextManager.h"
#include "Common/IRequestDispatcher.h"

namespace KxVFS
{
	class KxVFS_API FileSystemService;
	class KxVFS_API BasicFileSystem;
	class KxVFS_API FileContextManager;
	class KxVFS_API IOManager;
	class KxVFS_API FileNode;
}

namespace KxVFS
{
	class KxVFS_API IFileSystem
	{
		friend class BasicFileSystem;
		friend class FileContextManager;
		friend class IOManager;

		public:
			static NTSTATUS GetNtStatusByWin32ErrorCode(DWORD errorCode);
			static NTSTATUS GetNtStatusByWin32LastErrorCode();
			static std::tuple<FileAttributes, CreationDisposition, AccessRights> MapKernelToUserCreateFileFlags(const EvtCreateFile& eventInfo);

			static bool UnMountDirectory(KxDynamicStringRefW mountPoint);

		protected:
			uint32_t ConvertDokanyOptions(FSFlags flags) const;
			bool IsWriteRequest(bool isExist, AccessRights desiredAccess, CreationDisposition createDisposition) const;
			bool IsWriteRequest(KxDynamicStringRefW filePath, AccessRights desiredAccess, CreationDisposition createDisposition) const;
			bool IsWriteRequest(const FileNode* node, AccessRights desiredAccess, CreationDisposition createDisposition) const
			{
				return IsWriteRequest(node != nullptr, desiredAccess, createDisposition);
			}

			bool IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const;
			void ProcessSESecurityPrivilege(PSECURITY_INFORMATION securityInformation) const;

			
			bool CheckAttributesToOverwriteFile(FileAttributes fileAttributes, FileAttributes requestAttributes, CreationDisposition creationDisposition) const;
			bool IsDirectory(ULONG kernelCreateOptions) const;

		public:
			virtual ~IFileSystem() = default;

		public:
			virtual FileSystemService& GetService() = 0;
			virtual IOManager& GetIOManager() = 0;
			virtual FileContextManager& GetFileContextManager() = 0;

			virtual bool IsMounted() const = 0;
			virtual FSError Mount() = 0;
			virtual bool UnMount() = 0;

			virtual KxDynamicStringW GetMountPoint() const = 0;
			virtual void SetMountPoint(KxDynamicStringRefW mountPoint) = 0;

			virtual KxDynamicStringW GetVolumeLabel() const = 0;
			virtual KxDynamicStringW GetVolumeFileSystem() const = 0;
			virtual uint32_t GetVolumeSerialNumber() const = 0;
			virtual NTSTATUS GetVolumeSizeInfo(int64_t& freeBytes, int64_t& totalSize) = 0;

		protected:
			// File system events
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

		protected:
			// Events for derived classes. Call them in your implementation to allow easier customization.
			virtual void OnFileCreated(EvtCreateFile& eventInfo, FileContext& fileContext) { }
			virtual void OnFileClosed(EvtCloseFile& eventInfo, FileContext& fileContext) { }
			virtual void OnFileCleanedUp(EvtCleanUp& eventInfo, FileContext& fileContext) { }
			virtual void OnFileDeleted(EvtCleanUp& eventInfo, FileContext& fileContext) { }
			
			virtual void OnFileRead(EvtReadFile& eventInfo, FileContext& fileContext) { }
			virtual void OnFileWritten(EvtWriteFile& eventInfo, FileContext& fileContext) { }

			virtual void OnFileBuffersFlushed(EvtFlushFileBuffers& eventInfo, FileContext& fileContext) { }
			virtual void OnAllocationSizeSet(EvtSetAllocationSize& eventInfo, FileContext& fileContext) { }
			virtual void OnEndOfFileSet(EvtSetEndOfFile& eventInfo, FileContext& fileContext) { }
			virtual void OnBasicFileInfoSet(EvtSetBasicFileInfo& eventInfo, FileContext& fileContext) { }
	};
}
