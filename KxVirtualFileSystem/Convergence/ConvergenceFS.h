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
			using RedirectionPathsListT = std::vector<KxDynamicStringW>;
			using DispatcherIndexT = std::unordered_map<KxDynamicStringW, KxDynamicStringW, std::hash<KxDynamicStringW>, Utility::Comparator::StringEqualToNoCase>;
			using NonExistentINIMapT = std::unordered_set<KxDynamicStringW>;
			using SearchDispatcherIndexT = std::unordered_map<KxDynamicStringW, TEnumerationVector>;

		public:
			using ExternalDispatcherIndexT = std::vector<std::pair<KxDynamicStringRefW, KxDynamicStringRefW>>;

		private:
			RedirectionPathsListT m_VirtualFolders;

			DispatcherIndexT m_RequestDispatcherIndex;
			mutable CriticalSection m_RequestDispatcherIndexCS;
		
			SearchDispatcherIndexT m_EnumerationDispatcherIndex;
			mutable CriticalSection m_EnumerationDispatcherIndexCS;

			mutable CriticalSection m_NonExistentINIFilesCS;
			NonExistentINIMapT m_NonExistentINIFiles;

		protected:
			void MakeFilePath(KxDynamicStringW& outPath, KxDynamicStringRefW folder, KxDynamicStringRefW file) const;
			bool IsPathInVirtualFolder(KxDynamicStringRefW fileName) const
			{
				return IsPathPresent(fileName);
			}
			bool IsPathPresent(KxDynamicStringRefW fileName, KxDynamicStringRefW* virtualFolder = nullptr) const;
			bool IsPathPresentInWriteTarget(KxDynamicStringRefW fileName) const;

			bool IsRequestToRoot(KxDynamicStringRefW fileName) const
			{
				return fileName.empty() || fileName == L"\\";
			}
			bool IsWriteRequest(KxDynamicStringRefW, ACCESS_MASK desiredAccess, DWORD createDisposition) const;
			bool IsReadRequest(KxDynamicStringRefW, ACCESS_MASK desiredAccess, DWORD createDisposition) const;
			bool IsDirectory(ULONG kernelCreateOptions) const;
			bool IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const;
			void ProcessSESecurityPrivilege(bool hasSESecurityPrivilege, PSECURITY_INFORMATION securityInformation) const;

			// IRequestDispatcher
			void ResolveLocation(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) override;
			bool TryDispatchRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) const;

			bool UpdateDispatcherIndexUnlocked(KxDynamicStringRefW, KxDynamicStringRefW targetPath);
			void UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath);

			bool UpdateDispatcherIndex(KxDynamicStringRefW requestedPath, KxDynamicStringRefW targetPath)
			{
				CriticalSectionLocker lock(m_RequestDispatcherIndexCS);
				return UpdateDispatcherIndexUnlocked(requestedPath, targetPath);
			}
			void UpdateDispatcherIndex(KxDynamicStringRefW requestedPath)
			{
				CriticalSectionLocker lock(m_RequestDispatcherIndexCS);
				UpdateDispatcherIndexUnlocked(requestedPath);
			}

			// IEnumerationDispatcher
			TEnumerationVector* GetEnumerationVector(KxDynamicStringRefW requestedPath) override;
			TEnumerationVector& CreateEnumerationVector(KxDynamicStringRefW requestedPath) override;
			void InvalidateEnumerationVector(KxDynamicStringRefW requestedPath) override;
			void InvalidateSearchDispatcherVectorForFile(KxDynamicStringRefW requestedFilePath);

			// Non-existent INI-files
			bool IsINIFile(KxDynamicStringRefW requestedPath) const;
			bool IsINIFileNonExistent(KxDynamicStringRefW requestedPath) const;
			void AddINIFile(KxDynamicStringRefW requestedPath);

			const RedirectionPathsListT& GetVirtualFolders() const
			{
				return m_VirtualFolders;
			}
			RedirectionPathsListT& GetVirtualFolders()
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
					KxDynamicStringW targetPath(L"\\\\?\\");
					targetPath += KxDynamicStringRefW(value.second.data(), value.second.size());
					UpdateDispatcherIndexUnlocked(KxDynamicStringRefW(value.first.data(), value.first.size()), targetPath);
				}
			}

		public:
			ConvergenceFS(Service* vfsService, KxDynamicStringRefW mountPoint, KxDynamicStringRefW writeTarget, uint32_t flags = DefFlags);
			virtual ~ConvergenceFS();

		public:
			virtual FSError Mount() override;
			virtual bool UnMount() override;

			KxDynamicStringRefW GetWriteTarget() const
			{
				return GetSource();
			}
			bool SetWriteTarget(KxDynamicStringRefW writeTarget);
		
			bool AddVirtualFolder(KxDynamicStringRefW path);
			bool ClearVirtualFolders();

			void BuildDispatcherIndex();
			void SetDispatcherIndex(const ExternalDispatcherIndexT& index)
			{
				SetDispatcherIndexT(index);
			}
			template<class T> void SetDispatcherIndex(const T& index)
			{
				SetDispatcherIndexT(index);
			}

		protected:
			template<class TEventInfo> void OnEvent(TEventInfo& eventInfo)
			{
				UpdateDispatcherIndex(eventInfo.FileName);
				InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
			}

			void OnFileClosed(EvtCloseFile& eventInfo, const KxDynamicStringW& tergetPath) override
			{
				OnEvent(eventInfo);
			}
			void OnFileCleanedUp(EvtCleanUp& eventInfo, const KxDynamicStringW& tergetPath) override
			{
				OnEvent(eventInfo);
			}
			void OnDirectoryDeleted(EvtCanDeleteFile& eventInfo, const KxDynamicStringW& tergetPath) override
			{
				OnEvent(eventInfo);
			}

		protected:
			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
			NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;

			NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
			NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;

			DWORD OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, Utility::StringSearcherHash& hashStore, TEnumerationVector* searchIndex);
			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
	};
}
