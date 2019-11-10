/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/DokanyFileSystem.h"
#include "KxVirtualFileSystem/MirrorFS.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API ConvergenceFS: public MirrorFS
	{
		private:
			using TVirtualFoldersVector = std::vector<KxDynamicStringW>;

		private:
			TVirtualFoldersVector m_VirtualFolders;
			mutable FileNode m_VirtualTree;

		protected:
			KxDynamicStringW MakeFilePath(KxDynamicStringRefW baseDirectory, KxDynamicStringRefW requestedPath, bool addNamespace = false) const;
			std::tuple<KxDynamicStringW, KxDynamicStringRefW> GetTargetPath(const FileNode* node, KxDynamicStringRefW requestedPath, bool addNamespace = false) const;
			KxDynamicStringW DispatchLocationRequest(KxDynamicStringRefW requestedPath) override;

			bool ProcessDeleteOnClose(Dokany2::DOKAN_FILE_INFO& fileInfo, FileNode& fileNode) const;
			bool IsWriteTargetNode(const FileNode& fileNode) const;

			const TVirtualFoldersVector& GetVirtualFolders() const
			{
				return m_VirtualFolders;
			}
			TVirtualFoldersVector& GetVirtualFolders()
			{
				return m_VirtualFolders;
			}

		public:
			ConvergenceFS(FileSystemService& service, KxDynamicStringRefW mountPoint = {}, KxDynamicStringRefW writeTarget = {}, FSFlags flags = FSFlags::None);

		public:
			FSError Mount() override;
			bool UnMount() override;

		public:
			KxDynamicStringRefW GetSource() const = delete;
			void SetSource(KxDynamicStringRefW source) = delete;

			KxDynamicStringRefW GetWriteTarget() const
			{
				return MirrorFS::GetSource();
			}
			void SetWriteTarget(KxDynamicStringRefW writeTarget)
			{
				MirrorFS::SetSource(writeTarget);
			}
			
			void AddVirtualFolder(KxDynamicStringRefW path);
			void ClearVirtualFolders();
			size_t BuildFileTree();

		protected:
			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode);
			NTSTATUS OnCreateDirectory(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode);
			
			NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;
			NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) override;
			NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) override;

			NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
			NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;

			NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;
			NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) override;

			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
			NTSTATUS OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) override;
			NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) override;

		protected:
			bool UpdateAttributes(FileContext& fileContext);

			void OnFileClosed(EvtCloseFile& eventInfo, FileContext& fileContext) override
			{
			}
			void OnFileCleanedUp(EvtCleanUp& eventInfo, FileContext& fileContext) override
			{
			}

			void OnFileWritten(EvtWriteFile& eventInfo, FileContext& fileContext) override
			{
				UpdateAttributes(fileContext);
			}
			void OnFileRead(EvtReadFile& eventInfo, FileContext& fileContext) override
			{
			}
			
			void OnFileBuffersFlushed(EvtFlushFileBuffers& eventInfo, FileContext& fileContext) override
			{
				UpdateAttributes(fileContext);
			}
			void OnAllocationSizeSet(EvtSetAllocationSize& eventInfo, FileContext& fileContext) override
			{
				UpdateAttributes(fileContext);
			}
			void OnEndOfFileSet(EvtSetEndOfFile& eventInfo, FileContext& fileContext)
			{
				UpdateAttributes(fileContext);
			}
			void OnBasicFileInfoSet(EvtSetBasicFileInfo& eventInfo, FileContext& fileContext)
			{
				UpdateAttributes(fileContext);
			}
	};
}
