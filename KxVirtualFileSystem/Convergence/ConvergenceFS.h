/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Mirror/MirrorFS.h"
#include "KxVirtualFileSystem/IRequestDispatcher.h"
#include "KxVirtualFileSystem/IEnumerationDispatcher.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class Service;

	class KxVFS_API ConvergenceFS: public MirrorFS, public IEnumerationDispatcher
	{
		private:
			using TVirtualFoldersVector = std::vector<KxDynamicStringW>;

			using TRequestDispatcherMap = Utility::Comparator::UnorderedMapNoCase<KxDynamicStringW>;
			using TSearchDispatcherMap = Utility::Comparator::UnorderedMapNoCase<TEnumerationVector>;
			using TNonExistentINIMap = Utility::Comparator::UnorderedSetNoCase;

		public:
			using TExternalDispatcherMap = std::vector<std::pair<KxDynamicStringRefW, KxDynamicStringRefW>>;

		private:
			TVirtualFoldersVector m_VirtualFolders;

			TRequestDispatcherMap m_RequestDispatcherIndex;
			mutable CriticalSection m_RequestDispatcherIndexCS;
		
			TSearchDispatcherMap m_EnumerationDispatcherIndex;
			mutable CriticalSection m_EnumerationDispatcherIndexCS;

			mutable CriticalSection m_NonExistentINIFilesCS;
			TNonExistentINIMap m_NonExistentINIFiles;

			bool m_IsINIOptimizationEnabled = false;

		protected:
			// IRequestDispatcher
			void MakeFilePath(KxDynamicStringW& outPath, KxDynamicStringRefW folder, KxDynamicStringRefW file) const;
			bool TryDispatchRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) const;
			void DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) override;

			bool UpdateDispatcherIndexUnlocked(KxDynamicStringRefW, KxDynamicStringRefW targetPath);
			void UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath);

			bool UpdateDispatcherIndex(KxDynamicStringRefW requestedPath, KxDynamicStringRefW targetPath);
			void UpdateDispatcherIndex(KxDynamicStringRefW requestedPath);

			// IEnumerationDispatcher
			TEnumerationVector* GetEnumerationVector(KxDynamicStringRefW requestedPath) override;
			TEnumerationVector& CreateEnumerationVector(KxDynamicStringRefW requestedPath) override;
			void InvalidateEnumerationVector(KxDynamicStringRefW requestedPath) override;
			void InvalidateSearchDispatcherVectorUsingFile(KxDynamicStringRefW requestedFilePath);

			// Non-existent INI-files
			bool IsINIFile(KxDynamicStringRefW requestedPath) const;
			bool IsINIFileNonExistent(KxDynamicStringRefW requestedPath) const;
			void AddINIFile(KxDynamicStringRefW requestedPath);
			void RemoveINIFile(KxDynamicStringRefW requestedPath);

			const TVirtualFoldersVector& GetVirtualFolders() const
			{
				return m_VirtualFolders;
			}
			TVirtualFoldersVector& GetVirtualFolders()
			{
				return m_VirtualFolders;
			}

			template<class T> void SetDispatcherIndexT(const T& index)
			{
				m_RequestDispatcherIndex.clear();
				m_EnumerationDispatcherIndex.clear();

				m_RequestDispatcherIndex.reserve(index.size());
				for (const auto& value: index)
				{
					KxDynamicStringW targetPath = Utility::LongPathPrefix;
					targetPath += KxDynamicStringRefW(value.second.data(), value.second.size());
					UpdateDispatcherIndexUnlocked(KxDynamicStringRefW(value.first.data(), value.first.size()), targetPath);
				}
			}

		public:
			ConvergenceFS(Service& service, KxDynamicStringRefW mountPoint = {}, KxDynamicStringRefW writeTarget = {}, uint32_t flags = DefFlags);
			virtual ~ConvergenceFS();

		public:
			FSError Mount() override;
			bool UnMount() override;

		public:
			KxDynamicStringRefW GetWriteTarget() const;
			void SetWriteTarget(KxDynamicStringRefW writeTarget);
		
			void AddVirtualFolder(KxDynamicStringRefW path);
			void ClearVirtualFolders();

			size_t BuildDispatcherIndex();
			void SetDispatcherIndex(const TExternalDispatcherMap& index)
			{
				SetDispatcherIndexT(index);
			}
			template<class T> void SetDispatcherIndex(const T& index)
			{
				SetDispatcherIndexT(index);
			}

			bool IsINIOptimizationEnabled() const;
			void EnableINIOptimization(bool value);

		protected:
			template<class TEventInfo> void OnEvent(TEventInfo& eventInfo)
			{
				UpdateDispatcherIndex(eventInfo.FileName);
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}

			void OnFileClosed(EvtCloseFile& eventInfo, const KxDynamicStringW& targetPath) override
			{
				OnEvent(eventInfo);
			}
			void OnFileCleanedUp(EvtCleanUp& eventInfo, const KxDynamicStringW& targetPath) override
			{
				OnEvent(eventInfo);
			}
			void OnDirectoryDeleted(EvtCanDeleteFile& eventInfo, const KxDynamicStringW& targetPath) override
			{
				OnEvent(eventInfo);
			}
			
			void OnFileWritten(EvtWriteFile& eventInfo, const KxDynamicStringW& targetPath) override
			{
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}
			void OnFileBuffersFlushed(EvtFlushFileBuffers& eventInfo, const KxDynamicStringW& targetPath)
			{
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}
			void OnAllocationSizeSet(EvtSetAllocationSize& eventInfo, const KxDynamicStringW& targetPath) override
			{
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}
			void OnEndOfFileSet(EvtSetEndOfFile& eventInfo, const KxDynamicStringW& targetPath)
			{
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}
			void OnBasicFileInfoSet(EvtSetBasicFileInfo& eventInfo, const KxDynamicStringW& targetPath)
			{
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
			}

		protected:
			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
			NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;

			DWORD OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, Utility::StringSearcherHash& hashStore, TEnumerationVector* searchIndex);
			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
	};
}
