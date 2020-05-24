#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Common/CallerUserImpersonation.h"
#include "KxVirtualFileSystem/Common/ExtendedSecurity.h"
#include "KxVirtualFileSystem/DokanyFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API MirrorFS:
		public DokanyFileSystem,
		public CallerUserImpersonation,
		public ExtendedSecurity,
		public IRequestDispatcher
	{
		private:
			DynamicStringW m_Source;

		protected:
			bool CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, DynamicStringRefW filePath) const;
			NtStatus CanDeleteDirectory(DynamicStringRefW directoryPath) const;

			DynamicStringW DispatchLocationRequest(DynamicStringRefW requestedPath) override;

		public:
			MirrorFS(FileSystemService& service, DynamicStringRefW mountPoint = {}, DynamicStringRefW source = {}, FSFlags flags = FSFlags::None);

		public:
			DynamicStringRefW GetSource() const;
			void SetSource(DynamicStringRefW source);

			NtStatus GetVolumeSizeInfo(int64_t& freeBytes, int64_t& totalSize) override;

		protected:
			NtStatus OnMount(EvtMounted& eventInfo) override;
			NtStatus OnUnMount(EvtUnMounted& eventInfo) override;

			NtStatus OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) override;
			NtStatus OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) override;
			NtStatus OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) override;

			NtStatus OnCreateFile(EvtCreateFile& eventInfo) override;
			NtStatus OnCloseFile(EvtCloseFile& eventInfo) override;
			NtStatus OnCleanUp(EvtCleanUp& eventInfo) override;
			NtStatus OnMoveFile(EvtMoveFile& eventInfo) override;
			NtStatus OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;

			NtStatus OnLockFile(EvtLockFile& eventInfo) override;
			NtStatus OnUnlockFile(EvtUnlockFile& eventInfo) override;

			NtStatus OnGetFileSecurity(EvtGetFileSecurity& eventInfo) override;
			NtStatus OnSetFileSecurity(EvtSetFileSecurity& eventInfo) override;

			NtStatus OnReadFile(EvtReadFile& eventInfo) override;
			NtStatus OnWriteFile(EvtWriteFile& eventInfo) override;
			
			NtStatus OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) override;
			NtStatus OnSetEndOfFile(EvtSetEndOfFile& eventInfo) override;
			NtStatus OnSetAllocationSize(EvtSetAllocationSize& eventInfo) override;
			NtStatus OnGetFileInfo(EvtGetFileInfo& eventInfo) override;
			NtStatus OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) override;

			NtStatus OnFindFiles(DynamicStringRefW path, DynamicStringRefW pattern, EvtFindFiles* event1, EvtFindFilesWithPattern* event2);
			NtStatus OnFindFiles(EvtFindFiles& eventInfo) override;
			NtStatus OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) override;
			NtStatus OnFindStreams(EvtFindStreams& eventInfo) override;
	};
}
