#pragma once
#include "Common.hpp"
#include "Utility.h"
#include "Misc/IncludeDokan.h"
#include "Common/FSError.h"
#include "Common/FSFlags.h"
#include "Common/FileNode.h"
#include "Common/IOManager.h"
#include "Common/FileContextManager.h"
#include "Common/IRequestDispatcher.h"
#include "Logger/ILogger.h"

namespace KxVFS
{
	class KxVFS_API FileSystemService;
	class KxVFS_API DokanyFileSystem;
	class KxVFS_API FileContextManager;
	class KxVFS_API IOManager;
	class KxVFS_API FileNode;
}

namespace KxVFS
{
	class KxVFS_API IFileSystem
	{
		friend class DokanyFileSystem;
		friend class FileContextManager;
		friend class IOManager;

		public:
			static NtStatus GetNtStatusByWin32ErrorCode(DWORD errorCode);
			static NtStatus GetNtStatusByWin32LastErrorCode();
			static std::tuple<FlagSet<FileAttributes>, CreationDisposition, FlagSet<AccessRights>> MapKernelToUserCreateFileFlags(const EvtCreateFile& eventInfo);

			static bool UnMountDirectory(DynamicStringRefW mountPoint);

		protected:
			bool IsWriteRequest(bool isExist, FlagSet<AccessRights> desiredAccess, CreationDisposition createDisposition) const;
			bool IsWriteRequest(DynamicStringRefW filePath, FlagSet<AccessRights> desiredAccess, CreationDisposition createDisposition) const;
			bool IsWriteRequest(const FileNode* node, FlagSet<AccessRights> desiredAccess, CreationDisposition createDisposition) const
			{
				return IsWriteRequest(node != nullptr, desiredAccess, createDisposition);
			}

			bool IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const;
			void ProcessSESecurityPrivilege(PSECURITY_INFORMATION securityInformation) const;

			bool CheckAttributesToOverwriteFile(FlagSet<FileAttributes> fileAttributes, FlagSet<FileAttributes> requestAttributes, CreationDisposition creationDisposition) const;
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

			virtual DynamicStringW GetMountPoint() const = 0;
			virtual void SetMountPoint(DynamicStringRefW mountPoint) = 0;

			virtual DynamicStringW GetVolumeLabel() const = 0;
			virtual DynamicStringW GetVolumeFileSystem() const = 0;
			virtual uint32_t GetVolumeSerialNumber() const = 0;
			virtual NtStatus GetVolumeSizeInfo(int64_t& freeBytes, int64_t& totalSize) = 0;

			virtual bool IsProcessCreatedInVFS(uint32_t pid) const = 0;

		protected:
			// File context saving and retrieving
			template<class TEventInfo>
			FileContext* GetFileContext(TEventInfo& eventInfo) const
			{
				return reinterpret_cast<FileContext*>(eventInfo.DokanFileInfo->Context);
			}
			
			template<class TEventInfo>
			FileContext* SaveFileContext(TEventInfo& eventInfo, FileContext* fileContext) const
			{
				eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(fileContext);
				return fileContext;
			}
			
			template<class TEventInfo>
			void ResetFileContext(TEventInfo& eventInfo) const
			{
				eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(nullptr);
			}

		protected:
			// File system events
			virtual NtStatus OnMount(EvtMounted& eventInfo) = 0;
			virtual NtStatus OnUnMount(EvtUnMounted& eventInfo) = 0;

			virtual NtStatus OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) = 0;
			virtual NtStatus OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) = 0;
			virtual NtStatus OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) = 0;

			virtual NtStatus OnCreateFile(EvtCreateFile& eventInfo) = 0;
			virtual NtStatus OnCloseFile(EvtCloseFile& eventInfo) = 0;
			virtual NtStatus OnCleanUp(EvtCleanUp& eventInfo) = 0;
			virtual NtStatus OnMoveFile(EvtMoveFile& eventInfo) = 0;
			virtual NtStatus OnCanDeleteFile(EvtCanDeleteFile& eventInfo) = 0;

			virtual NtStatus OnLockFile(EvtLockFile& eventInfo) = 0;
			virtual NtStatus OnUnlockFile(EvtUnlockFile& eventInfo) = 0;
			virtual NtStatus OnGetFileSecurity(EvtGetFileSecurity& eventInfo) = 0;
			virtual NtStatus OnSetFileSecurity(EvtSetFileSecurity& eventInfo) = 0;

			virtual NtStatus OnReadFile(EvtReadFile& eventInfo) = 0;
			virtual NtStatus OnWriteFile(EvtWriteFile& eventInfo) = 0;
			virtual NtStatus OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) = 0;
			virtual NtStatus OnSetEndOfFile(EvtSetEndOfFile& eventInfo) = 0;
			virtual NtStatus OnSetAllocationSize(EvtSetAllocationSize& eventInfo) = 0;
			virtual NtStatus OnGetFileInfo(EvtGetFileInfo& eventInfo) = 0;
			virtual NtStatus OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) = 0;

			virtual NtStatus OnFindFiles(EvtFindFiles& eventInfo) = 0;
			virtual NtStatus OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) = 0;
			virtual NtStatus OnFindStreams(EvtFindStreams& eventInfo) = 0;

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
