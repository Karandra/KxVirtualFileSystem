/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSIDispatcher.h"
#include "KxVFSISearchDispatcher.h"
#include "Mirror/KxVFSMirror.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxDynamicString.h"
#include "Utility/KxVFSContext.h"
class KxVFSService;

class KxVFS_API KxVFSConvergence: public KxVFSMirror, public KxVFSISearchDispatcher
{
	private:
		using RedirectionPathsListT = std::vector<std::wstring>;
		using DispatcherIndexT = std::unordered_map<KxDynamicString, KxDynamicString>;
		using NonExistentINIMapT = std::unordered_set<KxDynamicString>;
		using SearchDispatcherIndexT = std::unordered_map<KxDynamicString, SearchDispatcherVectorT>;

	public:
		using ExternalDispatcherIndexT = std::vector<std::pair<KxDynamicStringRef, KxDynamicStringRef>>;

	private:
		RedirectionPathsListT m_RedirectionPaths;
		bool m_CanDeleteInVirtualFolder = false;

		DispatcherIndexT m_DispathcerIndex;
		mutable KxVFSCriticalSection m_DispatcherIndexCS;
		
		SearchDispatcherIndexT m_SearchDispathcerIndex;
		mutable KxVFSCriticalSection m_SearchDispatcherIndexCS;

		mutable KxVFSCriticalSection m_NonExistentINIFilesCS;
		NonExistentINIMapT m_NonExistentINIFiles;

	protected:
		KxDynamicString MakeFilePath(const KxDynamicStringRef& folder, const KxDynamicStringRef& file) const;
		bool IsPathInVirtualFolder(const WCHAR* fileName) const
		{
			return IsPathPresent(fileName);
		}
		bool IsPathPresent(const WCHAR* fileName, const WCHAR** virtualFolder = NULL) const;
		bool IsPathPresentInWriteTarget(const WCHAR* fileName) const;

		bool IsRequestToRoot(const WCHAR* fileName) const
		{
			return fileName == NULL || (*fileName == L'\\' && *(fileName++)== L'\000');
		}
		bool IsWriteRequest(const WCHAR* filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const;
		bool IsReadRequest(const WCHAR* filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const;
		bool IsDirectory(ULONG kernelCreateOptions) const;
		bool IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const;
		void ProcessSESecurityPrivilege(bool hasSESecurityPrivilege, PSECURITY_INFORMATION securityInformation) const;

		// KxVFSIDispatcher
		virtual KxDynamicString GetTargetPath(const WCHAR* requestedPath) override;

		bool UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath, const KxDynamicString& targetPath);
		void UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath);

		bool UpdateDispatcherIndex(const KxDynamicString& requestedPath, const KxDynamicString& targetPath)
		{
			KxVFSCriticalSectionLocker lock(m_DispatcherIndexCS);
			return UpdateDispatcherIndexUnlocked(requestedPath, targetPath);
		}
		void UpdateDispatcherIndex(const KxDynamicString& requestedPath)
		{
			KxVFSCriticalSectionLocker lock(m_DispatcherIndexCS);
			UpdateDispatcherIndexUnlocked(requestedPath);
		}
		
		KxDynamicString TryDispatchRequest(const KxDynamicString& requestedPath) const;

		// KxVFSISearchDispatcher
		virtual SearchDispatcherVectorT* GetSearchDispatcherVector(const KxDynamicString& requestedPath) override;
		virtual SearchDispatcherVectorT* CreateSearchDispatcherVector(const KxDynamicString& requestedPath) override;
		virtual void InvalidateSearchDispatcherVector(const KxDynamicString& requestedPath) override;
		void InvalidateSearchDispatcherVectorForFile(const KxDynamicString& requestedFilePath)
		{
			KxDynamicString sFolderPath = requestedFilePath.before_last(TEXT('\\'));
			if (sFolderPath.empty())
			{
				sFolderPath = TEXT("\\");
			}
			InvalidateSearchDispatcherVector(sFolderPath);
		}

		// Non-existent INI-files
		bool IsINIFile(const KxDynamicStringRef& requestedPath) const;
		bool IsINIFileNonExistent(const KxDynamicStringRef& requestedPath) const;
		void AddINIFile(const KxDynamicStringRef& requestedPath);

	private:
		const RedirectionPathsListT& GetPaths() const
		{
			return m_RedirectionPaths;
		}
		RedirectionPathsListT& GetPaths()
		{
			return m_RedirectionPaths;
		}
		KxDynamicString& NormalizePath(KxDynamicString& requestedPath) const;
		KxDynamicString NormalizePath(const KxDynamicStringRef& requestedPath) const;

		template<class T> void SetDispatcherIndexT(const T& index)
		{
			m_DispathcerIndex.clear();
			m_SearchDispathcerIndex.clear();

			m_DispathcerIndex.reserve(index.size());
			for (const auto& value: index)
			{
				KxDynamicString targetPath(L"\\\\?\\");
				targetPath += KxDynamicStringRef(value.second.data(), value.second.size());
				UpdateDispatcherIndexUnlocked(KxDynamicStringRef(value.first.data(), value.first.size()), targetPath);
			}
		}

	public:
		KxVFSConvergence(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* writeTarget, ULONG falgs = DefFlags, ULONG requestTimeout = DefRequestTimeout);
		virtual ~KxVFSConvergence();

	public:
		virtual int Mount() override;
		virtual bool UnMount() override;

		const WCHAR* GetWriteTarget() const
		{
			return GetSource();
		}
		const KxDynamicStringRef GetWriteTargetRef() const
		{
			return GetSourceRef();
		}
		bool SetWriteTarget(const WCHAR* writeTarget);
		
		bool AddVirtualFolder(const WCHAR* path);
		bool ClearVirtualFolders();

		bool CanDeleteInVirtualFolder() const
		{
			return m_CanDeleteInVirtualFolder;
		}
		bool SetCanDeleteInVirtualFolder(bool value);

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
		virtual NTSTATUS OnMount(EvtMounted& eventInfo) override;
		virtual NTSTATUS OnUnMount(EvtUnMounted& eventInfo) override;

		virtual NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
		virtual NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) override;
		virtual NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) override;
		virtual NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;
		virtual NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;

		virtual NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) override;
		virtual NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) override;

		virtual NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
		virtual NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;

		DWORD OnFindFilesAux(const KxDynamicString& path, EvtFindFiles& eventInfo, KxVFSUtility::StringSearcherHash& hashStore, SearchDispatcherVectorT* searchIndex);
		virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
};
