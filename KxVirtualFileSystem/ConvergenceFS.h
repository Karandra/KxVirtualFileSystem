#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/DokanyFileSystem.h"
#include "KxVirtualFileSystem/MirrorFS.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API ConvergenceFS: public MirrorFS
	{
		private:
			using TVirtualFoldersVector = std::vector<DynamicStringW>;

		private:
			TVirtualFoldersVector m_VirtualFolders;
			mutable FileNode m_VirtualTree;

		protected:
			DynamicStringW MakeFilePath(DynamicStringRefW baseDirectory, DynamicStringRefW requestedPath, bool addNamespace = false) const;
			std::tuple<DynamicStringW, DynamicStringRefW> GetTargetPath(const FileNode* node, DynamicStringRefW requestedPath, bool addNamespace = false) const;
			DynamicStringW DispatchLocationRequest(DynamicStringRefW requestedPath) override;

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
			ConvergenceFS(FileSystemService& service, DynamicStringRefW mountPoint = {}, DynamicStringRefW writeTarget = {}, FSFlags flags = FSFlags::None);

		public:
			FSError Mount() override;
			bool UnMount() override;

		public:
			DynamicStringRefW GetSource() const = delete;
			void SetSource(DynamicStringRefW source) = delete;

			DynamicStringRefW GetWriteTarget() const
			{
				return MirrorFS::GetSource();
			}
			void SetWriteTarget(DynamicStringRefW writeTarget)
			{
				MirrorFS::SetSource(writeTarget);
			}
			
			void AddVirtualFolder(DynamicStringRefW path);
			void ClearVirtualFolders();
			size_t BuildFileTree();

		protected:
			NtStatus OnCreateFile(EvtCreateFile& eventInfo) override;
			NtStatus OnCreateFile(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode);
			NtStatus OnCreateDirectory(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode);
			
			NtStatus OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;
			NtStatus OnCloseFile(EvtCloseFile& eventInfo) override;
			NtStatus OnCleanUp(EvtCleanUp& eventInfo) override;

			NtStatus OnReadFile(EvtReadFile& eventInfo) override;
			NtStatus OnWriteFile(EvtWriteFile& eventInfo) override;

			NtStatus OnMoveFile(EvtMoveFile& eventInfo) override;
			NtStatus OnGetFileInfo(EvtGetFileInfo& eventInfo) override;

			NtStatus OnFindFiles(EvtFindFiles& eventInfo) override;
			NtStatus OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo) override;
			NtStatus OnFindStreams(EvtFindStreams& eventInfo) override;

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
