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
		KxDynamicString MakeFilePath(const KxDynamicStringRef& sFolder, const KxDynamicStringRef& sFile) const;
		bool IsPathInVirtualFolder(const WCHAR* sFileName) const
		{
			return IsPathPresent(sFileName);
		}
		bool IsPathPresent(const WCHAR* sFileName, const WCHAR** sVirtualFolder = NULL) const;
		bool IsPathPresentInWriteTarget(const WCHAR* sFileName) const;

		bool IsRequestToRoot(const WCHAR* pFileName) const
		{
			return pFileName == NULL || (*pFileName == L'\\' && *(pFileName++)== L'\000');
		}
		bool IsWriteRequest(const WCHAR* sFilePath, ACCESS_MASK nDesiredAccess, DWORD nCreateDisposition) const;
		bool IsReadRequest(const WCHAR* sFilePath, ACCESS_MASK nDesiredAccess, DWORD nCreateDisposition) const;
		bool IsDirectory(ULONG nKeCreateOptions) const;
		bool IsRequestingSACLInfo(const PSECURITY_INFORMATION pSecurityInformation) const;
		void ProcessSESecurityPrivilege(bool bHasSESecurityPrivilege, PSECURITY_INFORMATION pSecurityInformation) const;

		// KxVFSIDispatcher
		virtual KxDynamicString GetTargetPath(const WCHAR* sRequestedPath) override;
		bool UpdateDispatcherIndex(const KxDynamicString& sRequestedPath, const KxDynamicString& sTargetPath);
		void UpdateDispatcherIndex(const KxDynamicString& sRequestedPath);
		KxDynamicString TryDispatchRequest(const KxDynamicString& sRequestedPath) const;

		// KxVFSISearchDispatcher
		virtual SearchDispatcherVectorT* GetSearchDispatcherVector(const KxDynamicString& sRequestedPath) override;
		virtual SearchDispatcherVectorT* CreateSearchDispatcherVector(const KxDynamicString& sRequestedPath) override;
		virtual void InvalidateSearchDispatcherVector(const KxDynamicString& sRequestedPath) override;
		void InvalidateSearchDispatcherVectorForFile(const KxDynamicString& sRequestedFilePath)
		{
			KxDynamicString sFolderPath = sRequestedFilePath.before_last(TEXT('\\'));
			if (sFolderPath.empty())
			{
				sFolderPath = TEXT("\\");
			}
			InvalidateSearchDispatcherVector(sFolderPath);
		}

		// Non-existent INI-files
		bool IsINIFile(const KxDynamicStringRef& sRequestedPath) const;
		bool IsINIFileNonExistent(const KxDynamicStringRef& sRequestedPath) const;
		void AddINIFile(const KxDynamicStringRef& sRequestedPath);

	private:
		const RedirectionPathsListT& GetPaths() const
		{
			return m_RedirectionPaths;
		}
		RedirectionPathsListT& GetPaths()
		{
			return m_RedirectionPaths;
		}
		KxDynamicString& NormalizePath(KxDynamicString& sRequestedPath) const;
		KxDynamicString NormalizePath(const KxDynamicStringRef& sRequestedPath) const;

	public:
		KxVFSConvergence(KxVFSService* pVFSService, const WCHAR* sMountPoint, const WCHAR* sWriteTarget, ULONG nFalgs = DefFlags, ULONG nRequestTimeout = DefRequestTimeout);
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
		bool SetWriteTarget(const WCHAR* sWriteTarget);
		
		bool AddVirtualFolder(const WCHAR* sPath);
		bool ClearVirtualFolders();

		bool CanDeleteInVirtualFolder() const
		{
			return m_CanDeleteInVirtualFolder;
		}
		bool SetCanDeleteInVirtualFolder(bool bValue);

		void RefreshDispatcherIndex();

	protected:
		virtual NTSTATUS OnMount(DOKAN_MOUNTED_INFO* pEventInfo) override;
		virtual NTSTATUS OnUnMount(DOKAN_UNMOUNTED_INFO* pEventInfo) override;

		virtual NTSTATUS OnCreateFile(DOKAN_CREATE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCloseFile(DOKAN_CLOSE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCleanUp(DOKAN_CLEANUP_EVENT* pEventInfo) override;
		virtual NTSTATUS OnMoveFile(DOKAN_MOVE_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* pEventInfo) override;

		virtual NTSTATUS OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* pEventInfo) override;
		virtual NTSTATUS OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* pEventInfo) override;

		virtual NTSTATUS OnReadFile(DOKAN_READ_FILE_EVENT* pEventInfo) override;
		virtual NTSTATUS OnWriteFile(DOKAN_WRITE_FILE_EVENT* pEventInfo) override;

		DWORD OnFindFilesAux(const KxDynamicString& sPath, DOKAN_FIND_FILES_EVENT* pEventInfo, KxVFSUtility::StringSearcherHash& tHash, SearchDispatcherVectorT* pSearchIndex);
		virtual NTSTATUS OnFindFiles(DOKAN_FIND_FILES_EVENT* pEventInfo) override;
};
