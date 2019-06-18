/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Common/CallerUserImpersonation.h"
#include "KxVirtualFileSystem/Common/ExtendedSecurity.h"
#include "KxVirtualFileSystem/BasicFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API MirrorFS:
		public BasicFileSystem,
		public CallerUserImpersonation,
		public ExtendedSecurity,
		public IRequestDispatcher
	{
		private:
			KxDynamicStringW m_Source;

		protected:
			bool CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, KxDynamicStringRefW filePath) const;
			NTSTATUS CanDeleteDirectory(KxDynamicStringRefW directoryPath) const;

			KxDynamicStringW DispatchLocationRequest(KxDynamicStringRefW requestedPath) override;

		public:
			MirrorFS(FileSystemService& service, KxDynamicStringRefW mountPoint = {}, KxDynamicStringRefW source = {}, FSFlags flags = FSFlags::None);

		public:
			KxDynamicStringRefW GetSource() const;
			void SetSource(KxDynamicStringRefW source);

			NTSTATUS GetVolumeSizeInfo(int64_t& freeBytes, int64_t& totalSize) override;

		protected:
			NTSTATUS OnMount(EvtMounted& eventInfo) override;
			NTSTATUS OnUnMount(EvtUnMounted& eventInfo) override;

			NTSTATUS OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) override;
			NTSTATUS OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) override;
			NTSTATUS OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) override;

			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
			NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) override;
			NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) override;
			NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;
			NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;

			NTSTATUS OnLockFile(EvtLockFile& eventInfo) override;
			NTSTATUS OnUnlockFile(EvtUnlockFile& eventInfo) override;

			NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) override;
			NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) override;

			NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
			NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;
			
			NTSTATUS OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) override;
			NTSTATUS OnSetEndOfFile(EvtSetEndOfFile& eventInfo) override;
			NTSTATUS OnSetAllocationSize(EvtSetAllocationSize& eventInfo) override;
			NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) override;
			NTSTATUS OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) override;

			NTSTATUS OnFindFiles(KxDynamicStringRefW path, KxDynamicStringRefW pattern, EvtFindFiles* event1, EvtFindFilesWithPattern* event2);
			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
			NTSTATUS OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) override;
			NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) override;
	};
}
