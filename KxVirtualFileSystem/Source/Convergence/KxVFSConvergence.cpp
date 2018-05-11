#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "KxVFSConvergence.h"
#include "Mirror/KxVFSMirror.h"
#include "Mirror/KxVFSMirrorStructs.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxVFSFileHandle.h"
#include "Utility/KxFileFinder.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

KxDynamicString KxVFSConvergence::MakeFilePath(const KxDynamicStringRef& sFolder, const KxDynamicStringRef& sFile) const
{
	KxDynamicString sBuffer(L"\\\\?\\");
	sBuffer.append(sFolder);
	sBuffer.append(sFile);
	return sBuffer;
}
bool KxVFSConvergence::IsPathPresent(const WCHAR* sRequestedPath, const WCHAR** sVirtualFolder) const
{
	for (auto i = m_RedirectionPaths.crbegin(); i != m_RedirectionPaths.crend(); ++i)
	{
		KxDynamicString sPath = MakeFilePath(*i, sRequestedPath);
		if (KxVFSUtility::IsExist(sPath))
		{
			if (sVirtualFolder)
			{
				*sVirtualFolder = i->c_str();
			}
			return true;
		}
	}
	return false;
}
bool KxVFSConvergence::IsPathPresentInWriteTarget(const WCHAR* sRequestedPath) const
{
	return KxVFSUtility::IsExist(MakeFilePath(GetWriteTargetRef(), sRequestedPath));
}

bool KxVFSConvergence::IsWriteRequest(const WCHAR* sFilePath, ACCESS_MASK nDesiredAccess, DWORD nCreateDisposition) const
{
	/*
	https://stackoverflow.com/questions/14469607/difference-between-open-always-and-create-always-in-createfile-of-windows-api
	|                  When the file...
	This argument:           |             Exists            Does not exist
	-------------------------+-----------------------------------------------
	CREATE_ALWAYS            |            Truncates             Creates
	CREATE_NEW         +-----------+        Fails               Creates
	OPEN_ALWAYS     ===| does this |===>    Opens               Creates
	OPEN_EXISTING      +-----------+        Opens                Fails
	TRUNCATE_EXISTING        |            Truncates              Fails
	*/

	return
		(
			nDesiredAccess & GENERIC_WRITE ||
			nCreateDisposition == CREATE_ALWAYS ||
			(nCreateDisposition == CREATE_NEW && !KxVFSUtility::IsExist(sFilePath)) ||
			(nCreateDisposition == OPEN_ALWAYS && !KxVFSUtility::IsExist(sFilePath)) ||
			(nCreateDisposition == TRUNCATE_EXISTING && KxVFSUtility::IsExist(sFilePath))
			);
}
bool KxVFSConvergence::IsReadRequest(const WCHAR* sFilePath, ACCESS_MASK nDesiredAccess, DWORD nCreateDisposition) const
{
	return !IsWriteRequest(sFilePath, nDesiredAccess, nCreateDisposition);
}
bool KxVFSConvergence::IsDirectory(ULONG nKeCreateOptions) const
{
	return nKeCreateOptions & FILE_DIRECTORY_FILE;
}
bool KxVFSConvergence::IsRequestingSACLInfo(const PSECURITY_INFORMATION pSecurityInformation) const
{
	return ((*pSecurityInformation & SACL_SECURITY_INFORMATION) || (*pSecurityInformation & BACKUP_SECURITY_INFORMATION));
}
void KxVFSConvergence::ProcessSESecurityPrivilege(bool bHasSESecurityPrivilege, PSECURITY_INFORMATION pSecurityInformation) const
{
	if (!bHasSESecurityPrivilege)
	{
		*pSecurityInformation &= ~SACL_SECURITY_INFORMATION;
		*pSecurityInformation &= ~BACKUP_SECURITY_INFORMATION;
	}
}

// KxVFSIDispatcher
KxDynamicString KxVFSConvergence::GetTargetPath(const WCHAR* sRequestedPath)
{
	// If request to root return path to write target
	if (IsRequestToRoot(sRequestedPath))
	{
		return MakeFilePath(GetWriteTargetRef(), sRequestedPath);
	}
	else
	{
		// Try to dispatch normal request
		KxDynamicString sPath = TryDispatchRequest(sRequestedPath);
		if (!sPath.empty())
		{
			// File found in index
			return sPath;
		}
		else
		{
			// File tot found in index. Search it in write target and in all virtual folders
			// and save to index if it found.
			KxDynamicString sInWriteTarget = MakeFilePath(GetWriteTargetRef(), sRequestedPath);
			if (!KxVFSUtility::IsExist(sInWriteTarget))
			{
				for (auto i = m_RedirectionPaths.rbegin(); i != m_RedirectionPaths.rend(); ++i)
				{
					sPath = MakeFilePath(*i, sRequestedPath);
					if (KxVFSUtility::IsExist(sPath))
					{
						UpdateDispatcherIndex(sRequestedPath, sPath);
						return sPath;
					}
				}
			}

			UpdateDispatcherIndex(sRequestedPath, sInWriteTarget);
			return sInWriteTarget;
		}
	}
}
bool KxVFSConvergence::UpdateDispatcherIndex(const KxDynamicString& sRequestedPath, const KxDynamicString& sTargetPath)
{
	if (!sRequestedPath.empty() && sRequestedPath != TEXT("\\"))
	{
		KxVFSCriticalSectionLocker lock(m_DispatcherIndexCS);

		KxDynamicString sKey = sRequestedPath.to_lower();
		NormalizePath(sKey);

		bool bInserted = m_DispathcerIndex.insert_or_assign(sKey, sTargetPath).second;
		return bInserted;
	}
	return false;
}
void KxVFSConvergence::UpdateDispatcherIndex(const KxDynamicString& sRequestedPath)
{
	KxVFSCriticalSectionLocker lock(m_DispatcherIndexCS);

	KxDynamicString sKey = sRequestedPath.to_lower();
	NormalizePath(sKey);

	m_DispathcerIndex.erase(sKey);
}
KxDynamicString KxVFSConvergence::TryDispatchRequest(const KxDynamicString& sRequestedPath) const
{
	KxDynamicString sKey = sRequestedPath.to_lower();
	NormalizePath(sKey);

	auto it = m_DispathcerIndex.find(sKey);
	if (it != m_DispathcerIndex.end())
	{
		return it->second;
	}
	return KxDynamicString();
}

// KxVFSISearchDispatcher
KxVFSConvergence::SearchDispatcherVectorT* KxVFSConvergence::GetSearchDispatcherVector(const KxDynamicString& sRequestedPath)
{
	KxDynamicString sKey = sRequestedPath.to_lower();
	NormalizePath(sKey);

	auto it = m_SearchDispathcerIndex.find(sKey);
	if (it != m_SearchDispathcerIndex.end())
	{
		return &(it->second);
	}
	return NULL;
}
KxVFSConvergence::SearchDispatcherVectorT* KxVFSConvergence::CreateSearchDispatcherVector(const KxDynamicString& sRequestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_SearchDispatcherIndexCS);

	KxDynamicString sKey = sRequestedPath.to_lower();
	NormalizePath(sKey);

	auto it = m_SearchDispathcerIndex.insert_or_assign(sKey, SearchDispatcherVectorT());
	return &(it.first->second);
}
void KxVFSConvergence::InvalidateSearchDispatcherVector(const KxDynamicString& sRequestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_SearchDispatcherIndexCS);

	KxDynamicString sKey = sRequestedPath.to_lower();
	NormalizePath(sKey);

	m_SearchDispathcerIndex.erase(sKey);
}

// Non-existent INI-files
bool KxVFSConvergence::IsINIFile(const KxDynamicStringRef& sRequestedPath) const
{
	return DokanIsNameInExpression(L"*.ini", sRequestedPath.data(), TRUE);
}
bool KxVFSConvergence::IsINIFileNonExistent(const KxDynamicStringRef& sRequestedPath) const
{
	KxDynamicString sKey(sRequestedPath);
	sKey.make_lower();
	NormalizePath(sKey);

	return m_NonExistentINIFiles.count(sKey) != 0;
}
void KxVFSConvergence::AddINIFile(const KxDynamicStringRef& sRequestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_NonExistentINIFilesCS);

	KxDynamicString sKey(sRequestedPath);
	sKey.make_lower();
	NormalizePath(sKey);

	m_NonExistentINIFiles.insert(sKey);
}

KxDynamicString& KxVFSConvergence::NormalizePath(KxDynamicString& sRequestedPath) const
{
	if (!sRequestedPath.empty())
	{
		if (sRequestedPath.front() == L'\\')
		{
			sRequestedPath.erase(0, 1);
		}
		if (sRequestedPath.back() == L'\\')
		{
			sRequestedPath.erase(sRequestedPath.size() - 1, 1);
		}
	}
	return sRequestedPath;
}
KxDynamicString KxVFSConvergence::NormalizePath(const KxDynamicStringRef& sRequestedPath) const
{
	KxDynamicString sOut(sRequestedPath);
	NormalizePath(sOut);
	return sOut;
}

KxVFSConvergence::KxVFSConvergence(KxVFSService* pVFSService, const WCHAR* sMountPoint, const WCHAR* sWriteTarget, ULONG nFalgs, ULONG nRequestTimeout)
	:KxVFSMirror(pVFSService, sMountPoint, sWriteTarget, nFalgs, nRequestTimeout)
{
}
KxVFSConvergence::~KxVFSConvergence()
{
}

int KxVFSConvergence::Mount()
{
	if (!IsMounted())
	{
		m_DispathcerIndex.clear();
		m_DispathcerIndex.reserve(m_RedirectionPaths.size() * 32);

		m_SearchDispathcerIndex.clear();
		m_SearchDispathcerIndex.reserve(m_RedirectionPaths.size() * 128);

		m_NonExistentINIFiles.clear();

		RefreshDispatcherIndex();
		return KxVFSMirror::Mount();
	}
	return DOKAN_ERROR;
}
bool KxVFSConvergence::UnMount()
{
	return KxVFSMirror::UnMount();
}

bool KxVFSConvergence::SetWriteTarget(const WCHAR* sWriteTarget)
{
	return SetSource(sWriteTarget);
}

bool KxVFSConvergence::AddVirtualFolder(const WCHAR* sPath)
{
	if (!IsMounted())
	{
		KxDynamicString sTemp = NormalizePath(sPath);
		m_RedirectionPaths.emplace_back(sTemp.view());
		return true;
	}
	return false;
}
bool KxVFSConvergence::ClearVirtualFolders()
{
	if (!IsMounted())
	{
		m_RedirectionPaths.clear();
		m_DispathcerIndex.clear();
		return true;
	}
	return false;
}

bool KxVFSConvergence::SetCanDeleteInVirtualFolder(bool bValue)
{
	if (!IsMounted())
	{
		m_CanDeleteInVirtualFolder = bValue;
		return true;
	}
	return false;
}
void KxVFSConvergence::RefreshDispatcherIndex()
{
	m_DispathcerIndex.clear();
	m_SearchDispathcerIndex.clear();
	
	auto ExtractRequestPath = [](const KxDynamicStringRef& sVirtualFolder, const KxDynamicString& sTargetPath)
	{
		KxDynamicString sRequestedPath(sTargetPath);
		size_t nEraseOffset = 0;
		if (sRequestedPath.length() >= sVirtualFolder.length() && sRequestedPath[sVirtualFolder.length()] == TEXT('\\'))
		{
			nEraseOffset = 1;
		}
		sRequestedPath.erase(0, sVirtualFolder.length() + nEraseOffset);

		return sRequestedPath;
	};

	std::function<void(const KxDynamicStringRef& sVirtualFolder, const KxDynamicString& sPath)> Recurse;
	Recurse = [this, &Recurse, &ExtractRequestPath](const KxDynamicStringRef& sVirtualFolder, const KxDynamicString& sPath)
	{
		KxFileFinder tFinder(sPath, TEXT("*"));

		KxFileFinderItem tItem = tFinder.FindNext();
		while (tItem.IsOK())
		{
			if (tItem.IsNormalItem())
			{
				// Save to file dispatcher index
				KxDynamicString sTargetPath(L"\\\\?\\");
				sTargetPath += tItem.GetFullPath();
				UpdateDispatcherIndex(ExtractRequestPath(sVirtualFolder, tItem.GetFullPath()), sTargetPath);

				if (tItem.IsDirectory())
				{
					// Makes game crash
					#if 0
					// Save directory list to search index
					KxDynamicString sRequestPath = ExtractRequestPath(sVirtualFolder, tItem.GetFullPath());
					SearchDispatcherVectorT* pSearchIndex = GetSearchDispatcherVector(sRequestPath);
					if (!pSearchIndex)
					{
						pSearchIndex = CreateSearchDispatcherVector(sRequestPath);
					}

					// Enum this folder content
					KxFileFinder tFolderFinder(tItem.GetFullPath(), TEXT("*"));
					KxFileFinderItem tFolderItem = tFolderFinder.FindNext();
					while (tFolderItem.IsOK())
					{
						if (tFolderItem.IsNormalItem())
						{
							// Add only if there are no file or folder with such name
							KxDynamicString sNameL = tFolderItem.GetName().to_lower();
							if (std::none_of(pSearchIndex->begin(), pSearchIndex->end(), [&sNameL](const WIN32_FIND_DATAW& tFindData)
							{
								return sNameL == KxDynamicString(tFindData.cFileName).make_lower();
							}))
							{
								pSearchIndex->push_back(tFolderItem.GetAsWIN32_FIND_DATA());
							}
						}
						tFolderItem = tFolderFinder.FindNext();
					}
					#endif

					Recurse(sVirtualFolder, tItem.GetFullPath());
				}
			}
			tItem = tFinder.FindNext();
		}
	};

	Recurse(GetWriteTargetRef(), GetWriteTargetRef());
	for (auto i = m_RedirectionPaths.rbegin(); i != m_RedirectionPaths.rend(); ++i)
	{
		Recurse(*i, *i);
	}
}

//////////////////////////////////////////////////////////////////////////
NTSTATUS KxVFSConvergence::OnMount(DOKAN_MOUNTED_INFO* pEventInfo)
{
	return KxVFSMirror::OnMount(pEventInfo);
}
NTSTATUS KxVFSConvergence::OnUnMount(DOKAN_UNMOUNTED_INFO* pEventInfo)
{
	return KxVFSMirror::OnUnMount(pEventInfo);
}

NTSTATUS KxVFSConvergence::OnCreateFile(DOKAN_CREATE_FILE_EVENT* pEventInfo)
{
	// Non-existent INI files optimization (1)
	const HRESULT nINIOptimizationReturnCode = STATUS_OBJECT_PATH_NOT_FOUND;
	#if 0
	if (!pEventInfo->DokanFileInfo->IsDirectory && IsINIFileNonExistent(pEventInfo->FileName))
	{
		return nINIOptimizationReturnCode;
	}
	#endif

	DWORD nErrorCode = 0;
	NTSTATUS nStatusCode = STATUS_SUCCESS;
	KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);

	DWORD nFileAttributesAndFlags = 0;
	DWORD nCreationDisposition = 0;
	ACCESS_MASK nGenericDesiredAccess = DokanMapStandardToGenericAccess(pEventInfo->DesiredAccess);
	DokanMapKernelToUserCreateFileFlags(pEventInfo, &nFileAttributesAndFlags, &nCreationDisposition);

	// When filePath is a directory, needs to change the flag so that the file can be opened.
	DWORD nFileAttributes = ::GetFileAttributesW(sTargetPath);
	if (nFileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		if (nFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(pEventInfo->CreateOptions & FILE_NON_DIRECTORY_FILE))
			{
				// Needed by FindFirstFile to list files in it
				// TODO: use ReOpenFile in MirrorFindFiles to set share read temporary
				pEventInfo->DokanFileInfo->IsDirectory = TRUE;
				pEventInfo->ShareAccess |= FILE_SHARE_READ;
			}
			else
			{
				// FILE_NON_DIRECTORY_FILE - Cannot open a dir as a file
				return STATUS_FILE_IS_A_DIRECTORY;
			}
		}
	}

	#if KxVFS_USE_ASYNC_IO
	nFileAttributesAndFlags |= FILE_FLAG_OVERLAPPED;
	#endif

	SECURITY_DESCRIPTOR* pFileSecurity = NULL;
	if (wcscmp(pEventInfo->FileName, L"\\") != 0 && wcscmp(pEventInfo->FileName, L"/") != 0	&& nCreationDisposition != OPEN_EXISTING && nCreationDisposition != TRUNCATE_EXISTING)
	{
		// We only need security information if there's a possibility a new file could be created
		CreateNewSecurity(pEventInfo, sTargetPath, pEventInfo->SecurityContext.AccessState.SecurityDescriptor, reinterpret_cast<PSECURITY_DESCRIPTOR*>(&pFileSecurity));
	}

	SECURITY_ATTRIBUTES tSecurityAttributes;
	tSecurityAttributes.nLength = sizeof(tSecurityAttributes);
	tSecurityAttributes.lpSecurityDescriptor = pFileSecurity;
	tSecurityAttributes.bInheritHandle = FALSE;

	if (pEventInfo->DokanFileInfo->IsDirectory)
	{
		/* It is a create directory request */

		if (nCreationDisposition == CREATE_NEW || nCreationDisposition == OPEN_ALWAYS)
		{
			// We create folder
			DWORD nCreateFolderErrorCode = STATUS_SUCCESS;
			if (!KxVFSUtility::CreateFolderTree(sTargetPath, false, &tSecurityAttributes, &nCreateFolderErrorCode))
			{
				//nErrorCode = GetLastError();
				nErrorCode = nCreateFolderErrorCode;

				// Fail to create folder for OPEN_ALWAYS is not an error
				if (nErrorCode != ERROR_ALREADY_EXISTS || nCreationDisposition == CREATE_NEW)
				{
					nStatusCode = GetNtStatusByWin32ErrorCode(nErrorCode);
				}
			}
			else
			{
				// Invalidate containing folder content
				InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);
			}
		}

		if (nStatusCode == STATUS_SUCCESS)
		{
			// Check first if we're trying to open a file as a directory.
			if (nFileAttributes != INVALID_FILE_ATTRIBUTES && !(nFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (pEventInfo->CreateOptions & FILE_DIRECTORY_FILE))
			{
				return STATUS_NOT_A_DIRECTORY;
			}

			// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
			KxVFSFileHandle hFile = CreateFileW
			(
				sTargetPath,
				nGenericDesiredAccess,
				pEventInfo->ShareAccess,
				&tSecurityAttributes,
				OPEN_EXISTING,
				nFileAttributesAndFlags|FILE_FLAG_BACKUP_SEMANTICS,
				NULL
			);

			if (hFile.IsOK())
			{
				KxVFSMirror_FileHandle* pMirrorHandle = PopMirrorFileHandle(hFile);
				if (!pMirrorHandle)
				{
					SetLastError(ERROR_INTERNAL_ERROR);
					nStatusCode = STATUS_INTERNAL_ERROR;
					hFile.Close();
				}
				else
				{
					hFile.Release();
				}

				// Save the file handle in Context
				pEventInfo->DokanFileInfo->Context = (ULONG64)pMirrorHandle;

				// Open succeed but we need to inform the driver that the dir open and not created by returning STATUS_OBJECT_NAME_COLLISION
				if (nCreationDisposition == OPEN_ALWAYS && nFileAttributes != INVALID_FILE_ATTRIBUTES)
				{
					nStatusCode = STATUS_OBJECT_NAME_COLLISION;
				}
			}
			else
			{
				nStatusCode = GetNtStatusByWin32LastErrorCode();
			}
		}
	}
	else
	{
		/* It is a create file request */

		// Cannot overwrite a hidden or system file if flag not set
		if (nFileAttributes != INVALID_FILE_ATTRIBUTES && ((!(nFileAttributesAndFlags & FILE_ATTRIBUTE_HIDDEN) &&
			(nFileAttributes & FILE_ATTRIBUTE_HIDDEN)) || (!(nFileAttributesAndFlags & FILE_ATTRIBUTE_SYSTEM) &&
															(nFileAttributes & FILE_ATTRIBUTE_SYSTEM))) &&(pEventInfo->CreateDisposition == TRUNCATE_EXISTING ||
																										   pEventInfo->CreateDisposition == CREATE_ALWAYS))
		{
			nStatusCode = STATUS_ACCESS_DENIED;
		}
		else
		{
			// Truncate should always be used with write access
			if (nCreationDisposition == TRUNCATE_EXISTING)
			{
				nGenericDesiredAccess |= GENERIC_WRITE;
			}

			// Non-existent INI files optimization (2)
			if ((nCreationDisposition == OPEN_ALWAYS || nCreationDisposition == OPEN_EXISTING) && IsINIFile(pEventInfo->FileName))
			{
				// If file doesn't exist
				bool bNotExist = IsINIFileNonExistent(pEventInfo->FileName);
				if (bNotExist || !KxVFSUtility::IsFileExist(sTargetPath))
				{
					if (!bNotExist)
					{
						AddINIFile(pEventInfo->FileName);
					}

					nStatusCode = nINIOptimizationReturnCode;
					goto CleanUp;
				}
			}

			// If we are asked to create a file, try to create its folder first
			if (IsWriteRequest(sTargetPath, nGenericDesiredAccess, nCreationDisposition))
			{
				KxVFSUtility::CreateFolderTree(sTargetPath, true);
			}

			KxVFSFileHandle hFile = CreateFileW
			(
				sTargetPath,
				nGenericDesiredAccess, // GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
				pEventInfo->ShareAccess,
				&tSecurityAttributes,
				nCreationDisposition,
				nFileAttributesAndFlags, // |FILE_FLAG_NO_BUFFERING,
				NULL
			);
			nErrorCode = GetLastError();

			if (!hFile.IsOK())
			{
				nStatusCode = GetNtStatusByWin32ErrorCode(nErrorCode);
			}
			else
			{
				// Need to update FileAttributes with previous when Overwrite file
				if (nFileAttributes != INVALID_FILE_ATTRIBUTES && nCreationDisposition == TRUNCATE_EXISTING)
				{
					SetFileAttributesW(sTargetPath, nFileAttributesAndFlags|nFileAttributes);
				}

				// Invalidate folder content if file is created or overwritten
				if (nCreationDisposition == CREATE_NEW || nCreationDisposition == CREATE_ALWAYS || nCreationDisposition == OPEN_ALWAYS || nCreationDisposition == TRUNCATE_EXISTING)
				{
					InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);
				}

				KxVFSMirror_FileHandle* pMirrorHandle = PopMirrorFileHandle(hFile);
				if (!pMirrorHandle)
				{
					SetLastError(ERROR_INTERNAL_ERROR);
					nStatusCode = STATUS_INTERNAL_ERROR;
				}
				else
				{
					hFile.Release();

					// Save the file handle in Context
					pEventInfo->DokanFileInfo->Context = (ULONG64)pMirrorHandle;

					if (nCreationDisposition == OPEN_ALWAYS || nCreationDisposition == CREATE_ALWAYS)
					{
						if (nErrorCode == ERROR_ALREADY_EXISTS)
						{
							// Open succeed but we need to inform the driver
							// that the file open and not created by returning STATUS_OBJECT_NAME_COLLISION
							nStatusCode = STATUS_OBJECT_NAME_COLLISION;
						}
					}
				}
			}
		}
	}

	CleanUp:
	if (pFileSecurity)
	{
		DestroyPrivateObjectSecurity(reinterpret_cast<PSECURITY_DESCRIPTOR*>(&pFileSecurity));
	}
	return nStatusCode;
}
NTSTATUS KxVFSConvergence::OnCloseFile(DOKAN_CLOSE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(pMirrorHandle->Lock);

			pMirrorHandle->IsClosed = TRUE;
			if (pMirrorHandle->FileHandle && pMirrorHandle->FileHandle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pMirrorHandle->FileHandle);
				pMirrorHandle->FileHandle = NULL;

				if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(pEventInfo->FileName))
				{
					if (pEventInfo->DokanFileInfo->DeleteOnClose)
					{
						KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
						if (CheckDeleteOnClose(pEventInfo->DokanFileInfo, sTargetPath))
						{
							UpdateDispatcherIndex(pEventInfo->FileName);
							InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);
						}
					}
				}
			}
		}

		pEventInfo->DokanFileInfo->Context = NULL;
		PushMirrorFileHandle(pMirrorHandle);
		return STATUS_NOT_SUPPORTED;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnCleanUp(DOKAN_CLEANUP_EVENT* pEventInfo)
{
	/* This gets called BEFORE MirrorCloseFile(). Per the documentation:
	*
	* Receipt of the IRP_MJ_CLEANUP request indicates that the handle reference count on a file object has reached zero.
	* (In other words, all handles to the file object have been closed.)
	* Often it is sent when a user-mode application has called the Microsoft Win32 CloseHandle function
	* (or when a kernel-mode driver has called ZwClose) on the last outstanding handle to a file object.
	*
	* It is important to note that when all handles to a file object have been closed, this does not necessarily
	* mean that the file object is no longer being used. System components, such as the Cache Manager and the Memory Manager,
	* might hold outstanding references to the file object. These components can still read to or write from a file, even
	* after an IRP_MJ_CLEANUP request is received.
	*/
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(pMirrorHandle->Lock);

			CloseHandle(pMirrorHandle->FileHandle);
			pMirrorHandle->FileHandle = NULL;
			pMirrorHandle->IsCleanedUp = TRUE;

			if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(pEventInfo->FileName))
			{
				if (pEventInfo->DokanFileInfo->DeleteOnClose)
				{
					KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
					if (CheckDeleteOnClose(pEventInfo->DokanFileInfo, sTargetPath))
					{
						UpdateDispatcherIndex(pEventInfo->FileName);
						InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);
					}
				}
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnMoveFile(DOKAN_MOVE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		KxDynamicString sTargetPathOld = GetTargetPath(pEventInfo->FileName);
		KxDynamicString sTargetPathNew = GetTargetPath(pEventInfo->NewFileName);
		KxVFSUtility::CreateFolderTree(sTargetPathNew, true);

		bool bOK = ::MoveFileExW(sTargetPathOld, sTargetPathNew, MOVEFILE_COPY_ALLOWED|(pEventInfo->ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0));
		if (bOK)
		{
			UpdateDispatcherIndex(pEventInfo->FileName);
			UpdateDispatcherIndex(pEventInfo->NewFileName);

			InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);
			InvalidateSearchDispatcherVectorForFile(pEventInfo->NewFileName);

			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		BY_HANDLE_FILE_INFORMATION tFileInfo = {0};
		if (!GetFileInformationByHandle(pMirrorHandle->FileHandle, &tFileInfo))
		{
			return DokanNtStatusFromWin32(GetLastError());
		}
		if ((tFileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
		{
			return STATUS_CANNOT_DELETE;
		}

		if (pEventInfo->DokanFileInfo->IsDirectory)
		{
			if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(pEventInfo->FileName))
			{
				KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
				NTSTATUS nStatus = CanDeleteDirectory(sTargetPath);
				if (nStatus == STATUS_SUCCESS)
				{
					UpdateDispatcherIndex(pEventInfo->FileName);
					InvalidateSearchDispatcherVector(pEventInfo->FileName);
				}
				return nStatus;
			}
			else
			{
				return STATUS_CANNOT_DELETE;
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}

NTSTATUS KxVFSConvergence::OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* pEventInfo)
{
	// This doesn't work properly anyway
	return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS KxVFSConvergence::OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* pEventInfo)
{
	// And this can be problematic too
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KxVFSConvergence::OnReadFile(DOKAN_READ_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle *pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (!pMirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	bool bIsCleanedUp = false;
	bool bIsClosed = false;
	GetMirrorFileHandleState(pMirrorHandle, &bIsCleanedUp, &bIsClosed);

	if (bIsClosed)
	{
		return STATUS_FILE_CLOSED;
	}

	if (bIsCleanedUp)
	{
		KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
		KxVFSFileHandle hTempFile = CreateFileW(sTargetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hTempFile.IsOK())
		{
			return ReadFileSynchronous(pEventInfo, hTempFile);
		}
		return GetNtStatusByWin32LastErrorCode();
	}

	#if KxVFS_USE_ASYNC_IO
	KxVFSMirror_Overlapped* pOverlapped = PopMirrorOverlapped();
	if (!pOverlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	pOverlapped->InternalOverlapped.Offset = (DWORD)(pEventInfo->Offset & 0xffffffff);
	pOverlapped->InternalOverlapped.OffsetHigh = (DWORD)((pEventInfo->Offset >> 32) & 0xffffffff);
	pOverlapped->FileHandle = pMirrorHandle;
	pOverlapped->Context = pEventInfo;
	pOverlapped->IoType = MIRROR_IOTYPE_READ;

	StartThreadpoolIo(pMirrorHandle->IoCompletion);
	if (!ReadFile(pMirrorHandle->FileHandle, pEventInfo->Buffer, pEventInfo->NumberOfBytesToRead, &pEventInfo->NumberOfBytesRead, (LPOVERLAPPED)pOverlapped))
	{
		DWORD nErrorCode = GetLastError();
		if (nErrorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(pMirrorHandle->IoCompletion);
			return GetNtStatusByWin32ErrorCode(nErrorCode);
		}
	}

	return STATUS_PENDING;
	#else
	return ReadFileSynchronous(pEventInfo, pMirrorHandle->FileHandle);
	#endif
}
NTSTATUS KxVFSConvergence::OnWriteFile(DOKAN_WRITE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (!pMirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	bool bIsCleanedUp = false;
	bool bIsClosed = false;
	GetMirrorFileHandleState(pMirrorHandle, &bIsCleanedUp, &bIsClosed);
	if (bIsClosed)
	{
		return STATUS_FILE_CLOSED;
	}

	// This write most likely change file attributes
	InvalidateSearchDispatcherVectorForFile(pEventInfo->FileName);

	DWORD nFileSizeHigh = 0;
	DWORD nFileSizeLow = 0;
	UINT64 nFileSize = 0;
	if (bIsCleanedUp)
	{
		KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
		KxVFSFileHandle hTempFile = CreateFileW(sTargetPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (!hTempFile.IsOK())
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		nFileSizeLow = ::GetFileSize(hTempFile, &nFileSizeHigh);
		if (nFileSizeLow == INVALID_FILE_SIZE)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		nFileSize = ((UINT64)nFileSizeHigh << 32) | nFileSizeLow;

		// Need to check if its really needs to be 'pMirrorHandle->FileHandle' and not 'hTempFile'
		return WriteFileSynchronous(pEventInfo, pMirrorHandle->FileHandle, nFileSize);
	}

	nFileSizeLow = ::GetFileSize(pMirrorHandle->FileHandle, &nFileSizeHigh);
	if (nFileSizeLow == INVALID_FILE_SIZE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	nFileSize = ((UINT64)nFileSizeHigh << 32) | nFileSizeLow;

	#if KxVFS_USE_ASYNC_IO
	if (pEventInfo->DokanFileInfo->PagingIo)
	{
		if ((UINT64)pEventInfo->Offset >= nFileSize)
		{
			pEventInfo->NumberOfBytesWritten = 0;
			return STATUS_SUCCESS;
		}

		if (((UINT64)pEventInfo->Offset + pEventInfo->NumberOfBytesToWrite) > nFileSize)
		{
			UINT64 nBytes = nFileSize - pEventInfo->Offset;
			if (nBytes >> 32)
			{
				pEventInfo->NumberOfBytesToWrite = (DWORD)(nBytes & 0xFFFFFFFFUL);
			}
			else
			{
				pEventInfo->NumberOfBytesToWrite = (DWORD)nBytes;
			}
		}
	}

	KxVFSMirror_Overlapped* pOverlapped = PopMirrorOverlapped();
	if (!pOverlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	pOverlapped->InternalOverlapped.Offset = (DWORD)(pEventInfo->Offset & 0xffffffff);
	pOverlapped->InternalOverlapped.OffsetHigh = (DWORD)((pEventInfo->Offset >> 32) & 0xffffffff);
	pOverlapped->FileHandle = pMirrorHandle;
	pOverlapped->Context = pEventInfo;
	pOverlapped->IoType = MIRROR_IOTYPE_WRITE;

	StartThreadpoolIo(pMirrorHandle->IoCompletion);
	if (!WriteFile(pMirrorHandle->FileHandle, pEventInfo->Buffer, pEventInfo->NumberOfBytesToWrite, &pEventInfo->NumberOfBytesWritten, (LPOVERLAPPED)pOverlapped))
	{
		DWORD nErrorCode = GetLastError();
		if (nErrorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(pMirrorHandle->IoCompletion);
			return GetNtStatusByWin32ErrorCode(nErrorCode);
		}
	}
	return STATUS_PENDING;
	#else
	return WriteFileSynchronous(pEventInfo, pMirrorHandle->FileHandle, nFileSize);
	#endif
}

DWORD KxVFSConvergence::OnFindFilesAux(const KxDynamicString& sPath, DOKAN_FIND_FILES_EVENT* pEventInfo, KxVFSUtility::StringSearcherHash& tHash, SearchDispatcherVectorT* pSearchIndex)
{
	DWORD nErrorCode = ERROR_NO_MORE_FILES;

	WIN32_FIND_DATAW tFindData = {0};
	HANDLE hFind = FindFirstFileW(sPath, &tFindData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		KxDynamicString sFileName;
		do
		{
			// Hash only lowercase version of name
			sFileName.assign(tFindData.cFileName);
			sFileName.make_lower();

			// If this file is not found already
			size_t nHash = KxVFSUtility::HashString(sFileName);
			if (tHash.emplace(nHash).second)
			{
				pEventInfo->FillFindData(pEventInfo, &tFindData);
				pSearchIndex->push_back(tFindData);

				// Save this path into dispatcher index.
				KxDynamicString sRequestedPath(pEventInfo->PathName);
				if (!sRequestedPath.empty() && sRequestedPath.back() != TEXT('\\'))
				{
					sRequestedPath += TEXT('\\');
				}
				sRequestedPath += tFindData.cFileName;

				// Remove '*' at end.
				KxDynamicString sTargetPath(sPath);
				sTargetPath.erase(sTargetPath.length() - 1, 1);
				sTargetPath += tFindData.cFileName;

				UpdateDispatcherIndex(sRequestedPath, sTargetPath);
			}
		}
		while (FindNextFileW(hFind, &tFindData));
		nErrorCode = GetLastError();

		FindClose(hFind);
	}
	return nErrorCode;
}
NTSTATUS KxVFSConvergence::OnFindFiles(DOKAN_FIND_FILES_EVENT* pEventInfo)
{
	KxVFSCriticalSectionLocker lockIndex(m_SearchDispatcherIndexCS);
	
	SearchDispatcherVectorT* pSearchIndex = GetSearchDispatcherVector(pEventInfo->PathName);
	if (pSearchIndex)
	{
		SendDispatcherVector(pEventInfo, *pSearchIndex);
		return STATUS_SUCCESS;
	}
	else
	{
		// CreateSearchDispatcherVector lock this CritSection so leave it now
		lockIndex.Leave();

		pSearchIndex = CreateSearchDispatcherVector(pEventInfo->PathName);
	}

	//////////////////////////////////////////////////////////////////////////
	auto AppendAsterix = [](KxDynamicString& sPath)
	{
		if (!sPath.empty())
		{
			if (sPath.back() != TEXT('\\'))
			{
				sPath += TEXT('\\');
			}
			sPath += TEXT('*');
		}
	};

	DWORD nErrorCode = 0;
	KxVFSUtility::StringSearcherHash tFoundPaths = {KxVFSUtility::HashString(L"."), KxVFSUtility::HashString(L"..")};

	// Find everything in write target first as it have highest priority
	KxDynamicString sWriteTarget = MakeFilePath(GetWriteTargetRef(), pEventInfo->PathName);
	AppendAsterix(sWriteTarget);

	nErrorCode = OnFindFilesAux(sWriteTarget, pEventInfo, tFoundPaths, pSearchIndex);

	// Then in other folders
	for (auto i = m_RedirectionPaths.crbegin(); i != m_RedirectionPaths.crend(); ++i)
	{
		KxDynamicString sPath = MakeFilePath(*i, pEventInfo->PathName);
		AppendAsterix(sPath);

		nErrorCode = OnFindFilesAux(sPath, pEventInfo, tFoundPaths, pSearchIndex);
	}

	if (nErrorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
