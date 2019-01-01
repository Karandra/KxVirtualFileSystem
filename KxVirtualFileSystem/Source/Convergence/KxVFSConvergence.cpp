/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "KxVFSConvergence.h"
#include "Mirror/KxVFSMirror.h"
#include "Mirror/KxVFSMirrorStructs.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxVFSFileHandle.h"
#include "Utility/KxFileFinder.h"
#include "Utility/KxCallAtScopeExit.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

KxDynamicString KxVFSConvergence::MakeFilePath(const KxDynamicStringRef& folder, const KxDynamicStringRef& file) const
{
	KxDynamicString buffer(L"\\\\?\\");
	buffer.append(folder);
	buffer.append(file);
	return buffer;
}
bool KxVFSConvergence::IsPathPresent(const WCHAR* requestedPath, const WCHAR** virtualFolder) const
{
	for (auto i = m_RedirectionPaths.crbegin(); i != m_RedirectionPaths.crend(); ++i)
	{
		KxDynamicString path = MakeFilePath(*i, requestedPath);
		if (KxVFSUtility::IsExist(path))
		{
			if (virtualFolder)
			{
				*virtualFolder = i->c_str();
			}
			return true;
		}
	}
	return false;
}
bool KxVFSConvergence::IsPathPresentInWriteTarget(const WCHAR* requestedPath) const
{
	return KxVFSUtility::IsExist(MakeFilePath(GetWriteTargetRef(), requestedPath));
}

bool KxVFSConvergence::IsWriteRequest(const WCHAR* filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
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
			desiredAccess & GENERIC_WRITE ||
			createDisposition == CREATE_ALWAYS ||
			(createDisposition == CREATE_NEW && !KxVFSUtility::IsExist(filePath)) ||
			(createDisposition == OPEN_ALWAYS && !KxVFSUtility::IsExist(filePath)) ||
			(createDisposition == TRUNCATE_EXISTING && KxVFSUtility::IsExist(filePath))
			);
}
bool KxVFSConvergence::IsReadRequest(const WCHAR* filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
{
	return !IsWriteRequest(filePath, desiredAccess, createDisposition);
}
bool KxVFSConvergence::IsDirectory(ULONG kernelCreateOptions) const
{
	return kernelCreateOptions & FILE_DIRECTORY_FILE;
}
bool KxVFSConvergence::IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const
{
	return ((*securityInformation & SACL_SECURITY_INFORMATION) || (*securityInformation & BACKUP_SECURITY_INFORMATION));
}
void KxVFSConvergence::ProcessSESecurityPrivilege(bool hasSESecurityPrivilege, PSECURITY_INFORMATION securityInformation) const
{
	if (!hasSESecurityPrivilege)
	{
		*securityInformation &= ~SACL_SECURITY_INFORMATION;
		*securityInformation &= ~BACKUP_SECURITY_INFORMATION;
	}
}

// KxVFSIDispatcher
KxDynamicString KxVFSConvergence::GetTargetPath(const WCHAR* requestedPath)
{
	// If request to root return path to write target
	if (IsRequestToRoot(requestedPath))
	{
		return MakeFilePath(GetWriteTargetRef(), requestedPath);
	}
	else
	{
		// Try to dispatch normal request
		KxDynamicString path = TryDispatchRequest(requestedPath);
		if (!path.empty())
		{
			// File found in index
			return path;
		}
		else
		{
			// File tot found in index. Search it in write target and in all virtual folders
			// and save to index if it found.
			KxDynamicString inWriteTarget = MakeFilePath(GetWriteTargetRef(), requestedPath);
			if (!KxVFSUtility::IsExist(inWriteTarget))
			{
				for (auto i = m_RedirectionPaths.rbegin(); i != m_RedirectionPaths.rend(); ++i)
				{
					path = MakeFilePath(*i, requestedPath);
					if (KxVFSUtility::IsExist(path))
					{
						UpdateDispatcherIndex(requestedPath, path);
						return path;
					}
				}
			}

			UpdateDispatcherIndex(requestedPath, inWriteTarget);
			return inWriteTarget;
		}
	}
}

bool KxVFSConvergence::UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath, const KxDynamicString& targetPath)
{
	if (!requestedPath.empty() && requestedPath != TEXT("\\"))
	{
		KxDynamicString key = requestedPath.to_lower();
		NormalizePath(key);

		bool isInserted = m_DispathcerIndex.insert_or_assign(key, targetPath).second;
		return isInserted;
	}
	return false;
}
void KxVFSConvergence::UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath)
{
	KxDynamicString key = requestedPath.to_lower();
	NormalizePath(key);

	m_DispathcerIndex.erase(key);
}

KxDynamicString KxVFSConvergence::TryDispatchRequest(const KxDynamicString& requestedPath) const
{
	KxDynamicString key = requestedPath.to_lower();
	NormalizePath(key);

	auto it = m_DispathcerIndex.find(key);
	if (it != m_DispathcerIndex.end())
	{
		return it->second;
	}
	return KxDynamicString();
}

// KxVFSISearchDispatcher
KxVFSConvergence::SearchDispatcherVectorT* KxVFSConvergence::GetSearchDispatcherVector(const KxDynamicString& requestedPath)
{
	KxDynamicString key = requestedPath.to_lower();
	NormalizePath(key);

	auto it = m_SearchDispathcerIndex.find(key);
	if (it != m_SearchDispathcerIndex.end())
	{
		return &(it->second);
	}
	return NULL;
}
KxVFSConvergence::SearchDispatcherVectorT* KxVFSConvergence::CreateSearchDispatcherVector(const KxDynamicString& requestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_SearchDispatcherIndexCS);

	KxDynamicString key = requestedPath.to_lower();
	NormalizePath(key);

	auto it = m_SearchDispathcerIndex.insert_or_assign(key, SearchDispatcherVectorT());
	return &(it.first->second);
}
void KxVFSConvergence::InvalidateSearchDispatcherVector(const KxDynamicString& requestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_SearchDispatcherIndexCS);

	KxDynamicString key = requestedPath.to_lower();
	NormalizePath(key);

	m_SearchDispathcerIndex.erase(key);
}

// Non-existent INI-files
bool KxVFSConvergence::IsINIFile(const KxDynamicStringRef& requestedPath) const
{
	return Dokany2::DokanIsNameInExpression(L"*.ini", requestedPath.data(), TRUE);
}
bool KxVFSConvergence::IsINIFileNonExistent(const KxDynamicStringRef& requestedPath) const
{
	KxDynamicString key(requestedPath);
	key.make_lower();
	NormalizePath(key);

	return m_NonExistentINIFiles.count(key) != 0;
}
void KxVFSConvergence::AddINIFile(const KxDynamicStringRef& requestedPath)
{
	KxVFSCriticalSectionLocker lockWrite(m_NonExistentINIFilesCS);

	KxDynamicString key(requestedPath);
	key.make_lower();
	NormalizePath(key);

	m_NonExistentINIFiles.insert(key);
}

KxDynamicString& KxVFSConvergence::NormalizePath(KxDynamicString& requestedPath) const
{
	if (!requestedPath.empty())
	{
		if (requestedPath.front() == L'\\')
		{
			requestedPath.erase(0, 1);
		}
		if (requestedPath.back() == L'\\')
		{
			requestedPath.erase(requestedPath.size() - 1, 1);
		}
	}
	return requestedPath;
}
KxDynamicString KxVFSConvergence::NormalizePath(const KxDynamicStringRef& requestedPath) const
{
	KxDynamicString out(requestedPath);
	NormalizePath(out);
	return out;
}

KxVFSConvergence::KxVFSConvergence(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* writeTarget, ULONG falgs, ULONG requestTimeout)
	:KxVFSMirror(vfsService, mountPoint, writeTarget, falgs, requestTimeout)
{
}
KxVFSConvergence::~KxVFSConvergence()
{
}

int KxVFSConvergence::Mount()
{
	if (!IsMounted())
	{
		m_DispathcerIndex.reserve(m_RedirectionPaths.size() * 32);
		m_SearchDispathcerIndex.reserve(m_RedirectionPaths.size() * 128);

		return KxVFSMirror::Mount();
	}
	return DOKAN_ERROR;
}
bool KxVFSConvergence::UnMount()
{
	m_DispathcerIndex.clear();
	m_SearchDispathcerIndex.clear();
	m_NonExistentINIFiles.clear();

	return KxVFSMirror::UnMount();
}

bool KxVFSConvergence::SetWriteTarget(const WCHAR* writeTarget)
{
	return SetSource(writeTarget);
}

bool KxVFSConvergence::AddVirtualFolder(const WCHAR* path)
{
	if (!IsMounted())
	{
		KxDynamicString temp = NormalizePath(path);
		m_RedirectionPaths.emplace_back(temp.view());
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

bool KxVFSConvergence::SetCanDeleteInVirtualFolder(bool value)
{
	if (!IsMounted())
	{
		m_CanDeleteInVirtualFolder = value;
		return true;
	}
	return false;
}
void KxVFSConvergence::BuildDispatcherIndex()
{
	m_DispathcerIndex.clear();
	m_SearchDispathcerIndex.clear();
	
	auto ExtractRequestPath = [](const KxDynamicStringRef& virtualFolder, const KxDynamicString& targetPath)
	{
		KxDynamicString requestedPath(targetPath);
		size_t eraseOffset = 0;
		if (requestedPath.length() >= virtualFolder.length() && requestedPath[virtualFolder.length()] == TEXT('\\'))
		{
			eraseOffset = 1;
		}
		requestedPath.erase(0, virtualFolder.length() + eraseOffset);

		return requestedPath;
	};

	std::function<void(const KxDynamicStringRef& virtualFolder, const KxDynamicString& path)> Recurse;
	Recurse = [this, &Recurse, &ExtractRequestPath](const KxDynamicStringRef& virtualFolder, const KxDynamicString& path)
	{
		KxFileFinder finder(path, TEXT("*"));

		KxFileFinderItem item = finder.FindNext();
		while (item.IsOK())
		{
			if (item.IsNormalItem())
			{
				// Save to file dispatcher index
				KxDynamicString targetPath(L"\\\\?\\");
				targetPath += item.GetFullPath();
				UpdateDispatcherIndexUnlocked(ExtractRequestPath(virtualFolder, item.GetFullPath()), targetPath);

				if (item.IsDirectory())
				{
					// Makes game crash
					#if 0
					// Save directory list to search index
					KxDynamicString sRequestPath = ExtractRequestPath(virtualFolder, item.GetFullPath());
					SearchDispatcherVectorT* searchIndex = GetSearchDispatcherVector(sRequestPath);
					if (!searchIndex)
					{
						searchIndex = CreateSearchDispatcherVector(sRequestPath);
					}

					// Enum this folder content
					KxFileFinder tFolderFinder(item.GetFullPath(), TEXT("*"));
					KxFileFinderItem tFolderItem = tFolderFinder.FindNext();
					while (tFolderItem.IsOK())
					{
						if (tFolderItem.IsNormalItem())
						{
							// Add only if there are no file or folder with such name
							KxDynamicString sNameL = tFolderItem.GetName().to_lower();
							if (std::none_of(searchIndex->begin(), searchIndex->end(), [&sNameL](const WIN32_FIND_DATAW& findData)
							{
								return sNameL == KxDynamicString(findData.cFileName).make_lower();
							}))
							{
								searchIndex->push_back(tFolderItem.GetAsWIN32_FIND_DATA());
							}
						}
						tFolderItem = tFolderFinder.FindNext();
					}
					#endif

					Recurse(virtualFolder, item.GetFullPath());
				}
			}
			item = finder.FindNext();
		}
	};

	Recurse(GetWriteTargetRef(), GetWriteTargetRef());
	for (auto i = m_RedirectionPaths.rbegin(); i != m_RedirectionPaths.rend(); ++i)
	{
		Recurse(*i, *i);
	}
}

//////////////////////////////////////////////////////////////////////////
NTSTATUS KxVFSConvergence::OnMount(EvtMounted& eventInfo)
{
	return KxVFSMirror::OnMount(eventInfo);
}
NTSTATUS KxVFSConvergence::OnUnMount(EvtUnMounted& eventInfo)
{
	return KxVFSMirror::OnUnMount(eventInfo);
}

NTSTATUS KxVFSConvergence::OnCreateFile(EvtCreateFile& eventInfo)
{
	// Non-existent INI files optimization
	const HRESULT iniOptimizationReturnCode = STATUS_OBJECT_PATH_NOT_FOUND;

	DWORD errorCode = 0;
	NTSTATUS statusCode = STATUS_SUCCESS;
	KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);

	DWORD fileAttributesAndFlags = 0;
	DWORD creationDisposition = 0;
	ACCESS_MASK genericDesiredAccess = 0;
	Dokany2::DokanMapKernelToUserCreateFileFlags(&eventInfo, &genericDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

	// When filePath is a directory, needs to change the flag so that the file can be opened.
	DWORD fileAttributes = ::GetFileAttributesW(targetPath);
	if (fileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(eventInfo.CreateOptions & FILE_NON_DIRECTORY_FILE))
			{
				// Needed by FindFirstFile to list files in it
				// TODO: use ReOpenFile in MirrorFindFiles to set share read temporary
				eventInfo.DokanFileInfo->IsDirectory = TRUE;
				eventInfo.ShareAccess |= FILE_SHARE_READ;
			}
			else
			{
				// FILE_NON_DIRECTORY_FILE - Cannot open a dir as a file
				return STATUS_FILE_IS_A_DIRECTORY;
			}
		}
	}

	#if KxVFS_USE_ASYNC_IO
	fileAttributesAndFlags |= FILE_FLAG_OVERLAPPED;
	#endif

	SECURITY_DESCRIPTOR* fileSecurity = NULL;
	KxCallAtScopeExit destroyFileSecurityAtExit([&fileSecurity]()
	{
		if (fileSecurity)
		{
			::DestroyPrivateObjectSecurity(reinterpret_cast<PSECURITY_DESCRIPTOR*>(&fileSecurity));
		}
	});

	if (wcscmp(eventInfo.FileName, L"\\") != 0 && wcscmp(eventInfo.FileName, L"/") != 0	&& creationDisposition != OPEN_EXISTING && creationDisposition != TRUNCATE_EXISTING)
	{
		// We only need security information if there's a possibility a new file could be created
		CreateNewSecurity(eventInfo, targetPath, eventInfo.SecurityContext.AccessState.SecurityDescriptor, reinterpret_cast<PSECURITY_DESCRIPTOR*>(&fileSecurity));
	}

	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = fileSecurity;
	securityAttributes.bInheritHandle = FALSE;

	if (eventInfo.DokanFileInfo->IsDirectory)
	{
		/* It is a create directory request */

		if (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS)
		{
			// We create folder
			DWORD nCreateFolderErrorCode = STATUS_SUCCESS;
			if (!KxVFSUtility::CreateFolderTree(targetPath, false, &securityAttributes, &nCreateFolderErrorCode))
			{
				//errorCode = GetLastError();
				errorCode = nCreateFolderErrorCode;

				// Fail to create folder for OPEN_ALWAYS is not an error
				if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CREATE_NEW)
				{
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
				}
			}
			else
			{
				// Invalidate containing folder content
				InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
			}
		}

		if (statusCode == STATUS_SUCCESS)
		{
			// Check first if we're trying to open a file as a directory.
			if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
			{
				return STATUS_NOT_A_DIRECTORY;
			}

			// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
			KxVFSFileHandle fileHandle = CreateFileW
			(
				targetPath,
				genericDesiredAccess,
				eventInfo.ShareAccess,
				&securityAttributes,
				OPEN_EXISTING,
				fileAttributesAndFlags|FILE_FLAG_BACKUP_SEMANTICS,
				NULL
			);

			if (fileHandle.IsOK())
			{
				KxVFSMirrorNS::FileHandle* mirrorHandle = PopMirrorFileHandle(fileHandle);
				if (!mirrorHandle)
				{
					SetLastError(ERROR_INTERNAL_ERROR);
					statusCode = STATUS_INTERNAL_ERROR;
					fileHandle.Close();
				}
				else
				{
					fileHandle.Release();
				}

				// Save the file handle in m_Context
				eventInfo.DokanFileInfo->Context = (ULONG64)mirrorHandle;

				// Open succeed but we need to inform the driver that the dir open and not created by returning STATUS_OBJECT_NAME_COLLISION
				if (creationDisposition == OPEN_ALWAYS && fileAttributes != INVALID_FILE_ATTRIBUTES)
				{
					statusCode = STATUS_OBJECT_NAME_COLLISION;
				}
			}
			else
			{
				statusCode = GetNtStatusByWin32LastErrorCode();
			}
		}
	}
	else
	{
		/* It is a create file request */

		// Cannot overwrite a hidden or system file if flag not set
		if (fileAttributes != INVALID_FILE_ATTRIBUTES && ((!(fileAttributesAndFlags & FILE_ATTRIBUTE_HIDDEN) &&
			(fileAttributes & FILE_ATTRIBUTE_HIDDEN)) || (!(fileAttributesAndFlags & FILE_ATTRIBUTE_SYSTEM) &&
															(fileAttributes & FILE_ATTRIBUTE_SYSTEM))) &&(eventInfo.CreateDisposition == TRUNCATE_EXISTING ||
																										   eventInfo.CreateDisposition == CREATE_ALWAYS))
		{
			statusCode = STATUS_ACCESS_DENIED;
		}
		else
		{
			// Truncate should always be used with write access
			if (creationDisposition == TRUNCATE_EXISTING)
			{
				genericDesiredAccess |= GENERIC_WRITE;
			}

			// Non-existent INI files optimization
			if ((creationDisposition == OPEN_ALWAYS || creationDisposition == OPEN_EXISTING) && IsINIFile(eventInfo.FileName))
			{
				// If file doesn't exist
				bool isNotExist = IsINIFileNonExistent(eventInfo.FileName);
				if (isNotExist || !KxVFSUtility::IsFileExist(targetPath))
				{
					if (!isNotExist)
					{
						AddINIFile(eventInfo.FileName);
					}

					statusCode = iniOptimizationReturnCode;
					
					// destroyFileSecurityAtExit will take care of everything else
					return statusCode;
				}
			}

			// If we are asked to create a file, try to create its folder first
			if (IsWriteRequest(targetPath, genericDesiredAccess, creationDisposition))
			{
				KxVFSUtility::CreateFolderTree(targetPath, true);
			}

			KxVFSFileHandle fileHandle = CreateFileW
			(
				targetPath,
				genericDesiredAccess, // GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
				eventInfo.ShareAccess,
				&securityAttributes,
				creationDisposition,
				fileAttributesAndFlags, // |FILE_FLAG_NO_BUFFERING,
				NULL
			);
			errorCode = GetLastError();

			if (!fileHandle.IsOK())
			{
				statusCode = GetNtStatusByWin32ErrorCode(errorCode);
			}
			else
			{
				// Need to update FileAttributes with previous when Overwrite file
				if (fileAttributes != INVALID_FILE_ATTRIBUTES && creationDisposition == TRUNCATE_EXISTING)
				{
					SetFileAttributesW(targetPath, fileAttributesAndFlags|fileAttributes);
				}

				// Invalidate folder content if file is created or overwritten
				if (creationDisposition == CREATE_NEW || creationDisposition == CREATE_ALWAYS || creationDisposition == OPEN_ALWAYS || creationDisposition == TRUNCATE_EXISTING)
				{
					InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
				}

				KxVFSMirrorNS::FileHandle* mirrorHandle = PopMirrorFileHandle(fileHandle);
				if (!mirrorHandle)
				{
					SetLastError(ERROR_INTERNAL_ERROR);
					statusCode = STATUS_INTERNAL_ERROR;
				}
				else
				{
					fileHandle.Release();

					// Save the file handle in m_Context
					eventInfo.DokanFileInfo->Context = (ULONG64)mirrorHandle;

					if (creationDisposition == OPEN_ALWAYS || creationDisposition == CREATE_ALWAYS)
					{
						if (errorCode == ERROR_ALREADY_EXISTS)
						{
							// Open succeed but we need to inform the driver
							// that the file open and not created by returning STATUS_OBJECT_NAME_COLLISION
							statusCode = STATUS_OBJECT_NAME_COLLISION;
						}
					}
				}
			}
		}
	}

	return statusCode;
}
NTSTATUS KxVFSConvergence::OnCloseFile(EvtCloseFile& eventInfo)
{
	KxVFSMirrorNS::FileHandle* mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (mirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->m_Lock);

			mirrorHandle->m_IsClosed = TRUE;
			if (mirrorHandle->m_FileHandle && mirrorHandle->m_FileHandle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(mirrorHandle->m_FileHandle);
				mirrorHandle->m_FileHandle = NULL;

				if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(eventInfo.FileName))
				{
					if (eventInfo.DokanFileInfo->DeleteOnClose)
					{
						KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);
						if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
						{
							UpdateDispatcherIndex(eventInfo.FileName);
							InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
						}
					}
				}
			}
		}

		eventInfo.DokanFileInfo->Context = NULL;
		PushMirrorFileHandle(mirrorHandle);
		return STATUS_NOT_SUPPORTED;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnCleanUp(EvtCleanUp& eventInfo)
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
	KxVFSMirrorNS::FileHandle* mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (mirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->m_Lock);

			CloseHandle(mirrorHandle->m_FileHandle);
			mirrorHandle->m_FileHandle = NULL;
			mirrorHandle->m_IsCleanedUp = TRUE;

			if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(eventInfo.FileName))
			{
				if (eventInfo.DokanFileInfo->DeleteOnClose)
				{
					KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);
					if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
					{
						UpdateDispatcherIndex(eventInfo.FileName);
						InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
					}
				}
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnMoveFile(EvtMoveFile& eventInfo)
{
	KxVFSMirrorNS::FileHandle* mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (mirrorHandle)
	{
		KxDynamicString targetPathOld = GetTargetPath(eventInfo.FileName);
		KxDynamicString targetPathNew = GetTargetPath(eventInfo.NewFileName);
		KxVFSUtility::CreateFolderTree(targetPathNew, true);

		bool isOK = ::MoveFileExW(targetPathOld, targetPathNew, MOVEFILE_COPY_ALLOWED|(eventInfo.ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0));
		if (isOK)
		{
			UpdateDispatcherIndex(eventInfo.FileName);
			UpdateDispatcherIndex(eventInfo.NewFileName);

			InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
			InvalidateSearchDispatcherVectorForFile(eventInfo.NewFileName);

			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSConvergence::OnCanDeleteFile(EvtCanDeleteFile& eventInfo)
{
	KxVFSMirrorNS::FileHandle* mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (mirrorHandle)
	{
		BY_HANDLE_FILE_INFORMATION fileInfo = {0};
		if (!GetFileInformationByHandle(mirrorHandle->m_FileHandle, &fileInfo))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
		{
			return STATUS_CANNOT_DELETE;
		}

		if (eventInfo.DokanFileInfo->IsDirectory)
		{
			if (CanDeleteInVirtualFolder() || !IsPathInVirtualFolder(eventInfo.FileName))
			{
				KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);
				NTSTATUS status = CanDeleteDirectory(targetPath);
				if (status == STATUS_SUCCESS)
				{
					UpdateDispatcherIndex(eventInfo.FileName);
					InvalidateSearchDispatcherVector(eventInfo.FileName);
				}
				return status;
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

NTSTATUS KxVFSConvergence::OnGetFileSecurity(EvtGetFileSecurity& eventInfo)
{
	// This doesn't work properly anyway
	return STATUS_NOT_IMPLEMENTED;
}
NTSTATUS KxVFSConvergence::OnSetFileSecurity(EvtSetFileSecurity& eventInfo)
{
	// And this can be problematic too
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS KxVFSConvergence::OnReadFile(EvtReadFile& eventInfo)
{
	KxVFSMirrorNS::FileHandle *mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (!mirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	bool isCleanedUp = false;
	bool isClosed = false;
	GetMirrorFileHandleState(mirrorHandle, &isCleanedUp, &isClosed);

	if (isClosed)
	{
		return STATUS_FILE_CLOSED;
	}

	if (isCleanedUp)
	{
		KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);
		KxVFSFileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (tempFileHandle.IsOK())
		{
			return ReadFileSynchronous(eventInfo, tempFileHandle);
		}
		return GetNtStatusByWin32LastErrorCode();
	}

	#if KxVFS_USE_ASYNC_IO
	KxVFSMirrorNS::Overlapped* overlapped = PopMirrorOverlapped();
	if (!overlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	overlapped->m_InternalOverlapped.Offset = (DWORD)(eventInfo.Offset & 0xffffffff);
	overlapped->m_InternalOverlapped.OffsetHigh = (DWORD)((eventInfo.Offset >> 32) & 0xffffffff);
	overlapped->m_FileHandle = mirrorHandle;
	overlapped->m_Context = &eventInfo;
	overlapped->m_IOType = KxVFSMirrorNS::MIRROR_IOTYPE_READ;

	StartThreadpoolIo(mirrorHandle->m_IOCompletion);
	if (!ReadFile(mirrorHandle->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToRead, &eventInfo.NumberOfBytesRead, (LPOVERLAPPED)overlapped))
	{
		DWORD errorCode = GetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(mirrorHandle->m_IOCompletion);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}

	return STATUS_PENDING;
	#else
	return ReadFileSynchronous(eventInfo, mirrorHandle->FileHandle);
	#endif
}
NTSTATUS KxVFSConvergence::OnWriteFile(EvtWriteFile& eventInfo)
{
	KxVFSMirrorNS::FileHandle* mirrorHandle = (KxVFSMirrorNS::FileHandle*)eventInfo.DokanFileInfo->Context;
	if (!mirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	bool isCleanedUp = false;
	bool isClosed = false;
	GetMirrorFileHandleState(mirrorHandle, &isCleanedUp, &isClosed);
	if (isClosed)
	{
		return STATUS_FILE_CLOSED;
	}

	// This write most likely change file attributes
	InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);

	DWORD fileSizeHigh = 0;
	DWORD fileSizeLow = 0;
	UINT64 fileSize = 0;
	if (isCleanedUp)
	{
		KxDynamicString targetPath = GetTargetPath(eventInfo.FileName);
		KxVFSFileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (!tempFileHandle.IsOK())
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		fileSizeLow = ::GetFileSize(tempFileHandle, &fileSizeHigh);
		if (fileSizeLow == INVALID_FILE_SIZE)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		fileSize = ((UINT64)fileSizeHigh << 32) | fileSizeLow;

		// Need to check if its really needs to be 'mirrorHandle->m_FileHandle' and not 'tempFileHandle'
		return WriteFileSynchronous(eventInfo, mirrorHandle->m_FileHandle, fileSize);
	}

	fileSizeLow = ::GetFileSize(mirrorHandle->m_FileHandle, &fileSizeHigh);
	if (fileSizeLow == INVALID_FILE_SIZE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	fileSize = ((UINT64)fileSizeHigh << 32) | fileSizeLow;

	#if KxVFS_USE_ASYNC_IO
	if (eventInfo.DokanFileInfo->PagingIo)
	{
		if ((UINT64)eventInfo.Offset >= fileSize)
		{
			eventInfo.NumberOfBytesWritten = 0;
			return STATUS_SUCCESS;
		}

		if (((UINT64)eventInfo.Offset + eventInfo.NumberOfBytesToWrite) > fileSize)
		{
			UINT64 bytes = fileSize - eventInfo.Offset;
			if (bytes >> 32)
			{
				eventInfo.NumberOfBytesToWrite = (DWORD)(bytes & 0xFFFFFFFFUL);
			}
			else
			{
				eventInfo.NumberOfBytesToWrite = (DWORD)bytes;
			}
		}
	}

	KxVFSMirrorNS::Overlapped* overlapped = PopMirrorOverlapped();
	if (!overlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	overlapped->m_InternalOverlapped.Offset = (DWORD)(eventInfo.Offset & 0xffffffff);
	overlapped->m_InternalOverlapped.OffsetHigh = (DWORD)((eventInfo.Offset >> 32) & 0xffffffff);
	overlapped->m_FileHandle = mirrorHandle;
	overlapped->m_Context = &eventInfo;
	overlapped->m_IOType = KxVFSMirrorNS::MIRROR_IOTYPE_WRITE;

	StartThreadpoolIo(mirrorHandle->m_IOCompletion);
	if (!WriteFile(mirrorHandle->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, &eventInfo.NumberOfBytesWritten, (LPOVERLAPPED)overlapped))
	{
		DWORD errorCode = GetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(mirrorHandle->m_IOCompletion);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}
	return STATUS_PENDING;
	#else
	return WriteFileSynchronous(eventInfo, mirrorHandle->FileHandle, fileSize);
	#endif
}

DWORD KxVFSConvergence::OnFindFilesAux(const KxDynamicString& path, EvtFindFiles& eventInfo, KxVFSUtility::StringSearcherHash& hashStore, SearchDispatcherVectorT* searchIndex)
{
	DWORD errorCode = ERROR_NO_MORE_FILES;

	WIN32_FIND_DATAW findData = {0};
	HANDLE findHandle = FindFirstFileW(path, &findData);
	if (findHandle != INVALID_HANDLE_VALUE)
	{
		KxDynamicString fileName;
		do
		{
			// Hash only lowercase version of name
			fileName.assign(findData.cFileName);
			fileName.make_lower();

			// If this file is not found already
			size_t hashValue = KxVFSUtility::HashString(fileName);
			if (hashStore.emplace(hashValue).second)
			{
				eventInfo.FillFindData(&eventInfo, &findData);
				searchIndex->push_back(findData);

				// Save this path into dispatcher index.
				KxDynamicString requestedPath(eventInfo.PathName);
				if (!requestedPath.empty() && requestedPath.back() != TEXT('\\'))
				{
					requestedPath += TEXT('\\');
				}
				requestedPath += findData.cFileName;

				// Remove '*' at end.
				KxDynamicString targetPath(path);
				targetPath.erase(targetPath.length() - 1, 1);
				targetPath += findData.cFileName;

				UpdateDispatcherIndex(requestedPath, targetPath);
			}
		}
		while (FindNextFileW(findHandle, &findData));
		errorCode = GetLastError();

		FindClose(findHandle);
	}
	return errorCode;
}
NTSTATUS KxVFSConvergence::OnFindFiles(EvtFindFiles& eventInfo)
{
	KxVFSCriticalSectionLocker lockIndex(m_SearchDispatcherIndexCS);
	
	SearchDispatcherVectorT* searchIndex = GetSearchDispatcherVector(eventInfo.PathName);
	if (searchIndex)
	{
		SendDispatcherVector(&eventInfo, *searchIndex);
		return STATUS_SUCCESS;
	}
	else
	{
		// CreateSearchDispatcherVector lock this CritSection so leave it now
		lockIndex.Leave();

		searchIndex = CreateSearchDispatcherVector(eventInfo.PathName);
	}

	//////////////////////////////////////////////////////////////////////////
	auto AppendAsterix = [](KxDynamicString& path)
	{
		if (!path.empty())
		{
			if (path.back() != TEXT('\\'))
			{
				path += TEXT('\\');
			}
			path += TEXT('*');
		}
	};

	DWORD errorCode = 0;
	KxVFSUtility::StringSearcherHash foundPaths = {KxVFSUtility::HashString(L"."), KxVFSUtility::HashString(L"..")};

	// Find everything in write target first as it have highest priority
	KxDynamicString writeTarget = MakeFilePath(GetWriteTargetRef(), eventInfo.PathName);
	AppendAsterix(writeTarget);

	errorCode = OnFindFilesAux(writeTarget, eventInfo, foundPaths, searchIndex);

	// Then in other folders
	for (auto i = m_RedirectionPaths.crbegin(); i != m_RedirectionPaths.crend(); ++i)
	{
		KxDynamicString path = MakeFilePath(*i, eventInfo.PathName);
		AppendAsterix(path);

		errorCode = OnFindFilesAux(path, eventInfo, foundPaths, searchIndex);
	}

	if (errorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
