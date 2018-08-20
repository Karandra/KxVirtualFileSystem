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
		bool UpdateDispatcherIndex(const KxDynamicString& requestedPath, const KxDynamicString& targetPath);
		void UpdateDispatcherIndex(const KxDynamicString& requestedPath);
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

		void RefreshDispatcherIndex();

	protected:
		virtual NTSTATUS OnMount(DOKAN_MOUNTED_INFO* eventInfo) override;
		virtual NTSTATUS OnUnMount(DOKAN_UNMOUNTED_INFO* eventInfo) override;

		virtual NTSTATUS OnCreateFile(DOKAN_CREATE_FILE_EVENT* eventInfo) override;
		virtual NTSTATUS OnCloseFile(DOKAN_CLOSE_FILE_EVENT* eventInfo) override;
		virtual NTSTATUS OnCleanUp(DOKAN_CLEANUP_EVENT* eventInfo) override;
		virtual NTSTATUS OnMoveFile(DOKAN_MOVE_FILE_EVENT* eventInfo) override;
		virtual NTSTATUS OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* eventInfo) override;

		virtual NTSTATUS OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* eventInfo) override;
		virtual NTSTATUS OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* eventInfo) override;

		virtual NTSTATUS OnReadFile(DOKAN_READ_FILE_EVENT* eventInfo) override;
		virtual NTSTATUS OnWriteFile(DOKAN_WRITE_FILE_EVENT* eventInfo) override;

		DWORD OnFindFilesAux(const KxDynamicString& path, DOKAN_FIND_FILES_EVENT* eventInfo, KxVFSUtility::StringSearcherHash& hashStore, SearchDispatcherVectorT* searchIndex);
		virtual NTSTATUS OnFindFiles(DOKAN_FIND_FILES_EVENT* eventInfo) override;
};
