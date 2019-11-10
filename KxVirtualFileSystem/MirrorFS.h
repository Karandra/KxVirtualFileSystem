/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
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
			KxDynamicStringW m_Source;

		protected:
			bool CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, KxDynamicStringRefW filePath) const;
			NtStatus CanDeleteDirectory(KxDynamicStringRefW directoryPath) const;

			KxDynamicStringW DispatchLocationRequest(KxDynamicStringRefW requestedPath) override;

		public:
			MirrorFS(FileSystemService& service, KxDynamicStringRefW mountPoint = {}, KxDynamicStringRefW source = {}, FSFlags flags = FSFlags::None);

		public:
			KxDynamicStringRefW GetSource() const;
			void SetSource(KxDynamicStringRefW source);

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

			NtStatus OnFindFiles(KxDynamicStringRefW path, KxDynamicStringRefW pattern, EvtFindFiles* event1, EvtFindFilesWithPattern* event2);
			NtStatus OnFindFiles(EvtFindFiles& eventInfo) override;
			NtStatus OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) override;
			NtStatus OnFindStreams(EvtFindStreams& eventInfo) override;
	};
}
