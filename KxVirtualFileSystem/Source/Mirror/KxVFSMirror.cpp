#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "KxVFSMirror.h"
#include "KxVFSMirrorStructs.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxVFSFileHandle.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

//////////////////////////////////////////////////////////////////////////
DWORD KxVFSMirror::GetParentSecurity(const WCHAR* sFilePath, PSECURITY_DESCRIPTOR* pParentSecurity) const
{
	int nLastPathSeparator = -1;
	for (int i = 0; i < MaxPathLength && sFilePath[i]; ++i)
	{
		if (sFilePath[i] == '\\')
		{
			nLastPathSeparator = i;
		}
	}
	if (nLastPathSeparator == -1)
	{
		return ERROR_PATH_NOT_FOUND;
	}

	WCHAR sParentPath[MaxPathLength];
	memcpy_s(sParentPath, MaxPathLength * sizeof(WCHAR), sFilePath, nLastPathSeparator * sizeof(WCHAR));
	sParentPath[nLastPathSeparator] = 0;

	// Must LocalFree() pParentSecurity
	return GetNamedSecurityInfoW
	(
		sParentPath,
		SE_FILE_OBJECT,
		BACKUP_SECURITY_INFORMATION, // give us everything
		NULL,
		NULL,
		NULL,
		NULL,
		pParentSecurity
	);
}
DWORD KxVFSMirror::CreateNewSecurity(DOKAN_CREATE_FILE_EVENT* pEventInfo, const WCHAR* sFilePath, PSECURITY_DESCRIPTOR pRequestedSecurity, PSECURITY_DESCRIPTOR* pNewSecurity) const
{
	if (!pEventInfo || !sFilePath || !pRequestedSecurity || !pNewSecurity)
	{
		return ERROR_INVALID_PARAMETER;
	}

	int nErrorCode = ERROR_SUCCESS;
	PSECURITY_DESCRIPTOR pParentDescriptor = NULL;
	if ((nErrorCode = GetParentSecurity(sFilePath, &pParentDescriptor)) == ERROR_SUCCESS)
	{
		static GENERIC_MAPPING g_GenericMapping =
		{
			FILE_GENERIC_READ,
			FILE_GENERIC_WRITE,
			FILE_GENERIC_EXECUTE,
			FILE_ALL_ACCESS
		};

		HANDLE hAaccessToken = DokanOpenRequestorToken(pEventInfo->DokanFileInfo);
		if (hAaccessToken && hAaccessToken != INVALID_HANDLE_VALUE)
		{
			if (!CreatePrivateObjectSecurity(pParentDescriptor,
											 pEventInfo->SecurityContext.AccessState.SecurityDescriptor,
											 pNewSecurity,
											 pEventInfo->DokanFileInfo->IsDirectory,
											 hAaccessToken,
											 &g_GenericMapping))
			{

				nErrorCode = GetLastError();
			}
			CloseHandle(hAaccessToken);
		}
		else
		{
			nErrorCode = GetLastError();
		}
		LocalFree(pParentDescriptor);
	}
	return nErrorCode;
}

bool KxVFSMirror::CheckDeleteOnClose(PDOKAN_FILE_INFO pFileInfo, const WCHAR* sFilePath) const
{
	if (pFileInfo->DeleteOnClose)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), sFilePath);

		// Should already be deleted by CloseHandle
		// if open with FILE_FLAG_DELETE_ON_CLOSE
		if (pFileInfo->IsDirectory)
		{
			RemoveDirectoryW(sFilePath);
		}
		else
		{
			DeleteFileW(sFilePath);
		}
		return true;
	}
	return false;
}
NTSTATUS KxVFSMirror::CanDeleteDirectory(const WCHAR* sFilePath) const
{
	KxDynamicString sFilePathCopy = sFilePath;

	size_t nFilePathLength = sFilePathCopy.length();
	if (sFilePathCopy[nFilePathLength - 1] != L'\\')
	{
		sFilePathCopy[nFilePathLength++] = L'\\';
	}

	sFilePathCopy[nFilePathLength] = L'*';
	sFilePathCopy[nFilePathLength + 1] = L'\0';

	WIN32_FIND_DATAW tFindData = {0};
	HANDLE hFind = FindFirstFileW(sFilePathCopy, &tFindData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	do
	{
		if (wcscmp(tFindData.cFileName, L"..") != 0 && wcscmp(tFindData.cFileName, L".") != 0)
		{
			FindClose(hFind);
			return STATUS_DIRECTORY_NOT_EMPTY;
		}
	}
	while (FindNextFileW(hFind, &tFindData) != 0);

	DWORD nErrorCode = GetLastError();
	if (nErrorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32ErrorCode(nErrorCode);
	}

	FindClose(hFind);
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::ReadFileSynchronous(DOKAN_READ_FILE_EVENT* pEventInfo, HANDLE hFileHandle) const
{
	LARGE_INTEGER tDistanceToMove;
	tDistanceToMove.QuadPart = pEventInfo->Offset;

	if (!SetFilePointerEx(hFileHandle, tDistanceToMove, NULL, FILE_BEGIN))
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	if (ReadFile(hFileHandle, pEventInfo->Buffer, pEventInfo->NumberOfBytesToRead, &pEventInfo->NumberOfBytesRead, NULL))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}
NTSTATUS KxVFSMirror::WriteFileSynchronous(DOKAN_WRITE_FILE_EVENT* pEventInfo, HANDLE hFileHandle, UINT64 nFileSize) const
{
	LARGE_INTEGER tDistanceToMove;

	if (pEventInfo->DokanFileInfo->WriteToEndOfFile)
	{
		LARGE_INTEGER tPos;
		tPos.QuadPart = 0;

		if (!SetFilePointerEx(hFileHandle, tPos, NULL, FILE_END))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
	}
	else
	{
		// Paging IO cannot write after allocate file size.
		if (pEventInfo->DokanFileInfo->PagingIo)
		{
			if ((UINT64)pEventInfo->Offset >= nFileSize)
			{
				pEventInfo->NumberOfBytesWritten = 0;
				return STATUS_SUCCESS;
			}

			// WFT is happening here?
			if (((UINT64)pEventInfo->Offset + pEventInfo->NumberOfBytesToWrite) > nFileSize)
			{
				UINT64 nBytes = nFileSize - pEventInfo->Offset;
				if (nBytes >> 32)
				{
					pEventInfo->NumberOfBytesToWrite = (DWORD)(nBytes & 0xFFFFFFFFUL);
				}
				else {

					pEventInfo->NumberOfBytesToWrite = (DWORD)nBytes;
				}
			}
		}

		if ((UINT64)pEventInfo->Offset > nFileSize)
		{
			// In the mirror sample helperZeroFileData is not necessary. NTFS will zero a hole.
			// But if user's file system is different from NTFS (or other Windows
			// file systems) then  users will have to zero the hole themselves.

			// If only Dokan devs can explain more clearly what they are talking about
		}

		tDistanceToMove.QuadPart = pEventInfo->Offset;
		if (!SetFilePointerEx(hFileHandle, tDistanceToMove, NULL, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
	}

	if (WriteFile(hFileHandle, pEventInfo->Buffer, pEventInfo->NumberOfBytesToWrite, &pEventInfo->NumberOfBytesWritten, NULL))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}

KxDynamicString KxVFSMirror::GetTargetPath(const WCHAR* sRequestedPath)
{
	KxDynamicString sTargetPath(L"\\\\?\\");
	sTargetPath.append(m_Source);
	sTargetPath.append(sRequestedPath);
	return sTargetPath;
}

//////////////////////////////////////////////////////////////////////////
KxVFSMirror::KxVFSMirror(KxVFSService* pVFSService, const WCHAR* sMountPoint, const WCHAR* sSource, ULONG nFalgs, ULONG nRequestTimeout)
	:KxVFSBase(pVFSService, sMountPoint, nFalgs, nRequestTimeout), m_Source(sSource)
{
}
KxVFSMirror::~KxVFSMirror()
{
}

bool KxVFSMirror::SetSource(const WCHAR* sSource)
{
	if (!IsMounted())
	{
		m_Source = sSource;
		return true;
	}
	return false;
}

int KxVFSMirror::Mount()
{
	if (!m_Source.empty())
	{
		#if KxVFS_USE_ASYNC_IO
		if (!InitializeAsyncIO())
		{
			return DOKAN_ERROR;
		}
		#endif
		InitializeMirrorFileHandles();
		return KxVFSBase::Mount();
	}
	return DOKAN_ERROR;
}

//////////////////////////////////////////////////////////////////////////
NTSTATUS KxVFSMirror::OnMount(DOKAN_MOUNTED_INFO* pEventInfo)
{
	return STATUS_NOT_SUPPORTED;
}
NTSTATUS KxVFSMirror::OnUnMount(DOKAN_UNMOUNTED_INFO* pEventInfo)
{
	CleanupMirrorFileHandles();
	#if KxVFS_USE_ASYNC_IO
	CleanupAsyncIO();
	#endif

	return STATUS_NOT_SUPPORTED;
}

NTSTATUS KxVFSMirror::OnGetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* pEventInfo)
{
	if (!GetDiskFreeSpaceExW(GetSource(), (ULARGE_INTEGER*)&pEventInfo->FreeBytesAvailable, (ULARGE_INTEGER*)&pEventInfo->TotalNumberOfBytes, (ULARGE_INTEGER*)&pEventInfo->TotalNumberOfFreeBytes))
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnGetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* pEventInfo)
{
	pEventInfo->VolumeInfo->VolumeSerialNumber = GetVolumeSerialNumber();
	pEventInfo->VolumeInfo->VolumeLabelLength = WriteString(GetVolumeName(), pEventInfo->VolumeInfo->VolumeLabel, pEventInfo->MaxLabelLengthInChars);
	pEventInfo->VolumeInfo->SupportsObjects = FALSE;

	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnGetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* pEventInfo)
{
	LPCWSTR sFileSystemName = GetVolumeFileSystemName();
	size_t maxFileSystemNameLengthInBytes = pEventInfo->MaxFileSystemNameLengthInChars * sizeof(WCHAR);

	pEventInfo->Attributes->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH|FILE_CASE_PRESERVED_NAMES|FILE_SUPPORTS_REMOTE_STORAGE|FILE_UNICODE_ON_DISK|FILE_PERSISTENT_ACLS|FILE_NAMED_STREAMS;
	pEventInfo->Attributes->MaximumComponentNameLength = 256; // Not 255, this means Dokan doesn't support long file names?

	WCHAR sVolumeRoot[4];
	sVolumeRoot[0] = GetSource()[0];
	sVolumeRoot[1] = ':';
	sVolumeRoot[2] = '\\';
	sVolumeRoot[3] = '\0';

	DWORD nFileSystemFlags = 0;
	DWORD nMaximumComponentLength = 0;

	WCHAR sFileSystemNameBuffer[255] = {0};
	if (GetVolumeInformationW(sVolumeRoot, NULL, 0, NULL, &nMaximumComponentLength, &nFileSystemFlags, sFileSystemNameBuffer, ARRAYSIZE(sFileSystemNameBuffer)))
	{
		pEventInfo->Attributes->MaximumComponentNameLength = nMaximumComponentLength;
		pEventInfo->Attributes->FileSystemAttributes &= nFileSystemFlags;

		sFileSystemName = sFileSystemNameBuffer;
	}
	pEventInfo->Attributes->FileSystemNameLength = WriteString(sFileSystemName, pEventInfo->Attributes->FileSystemName, pEventInfo->MaxFileSystemNameLengthInChars);

	return STATUS_SUCCESS;
}

NTSTATUS KxVFSMirror::OnCreateFile(DOKAN_CREATE_FILE_EVENT* pEventInfo)
{
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
		// It is a create directory request
		if (nCreationDisposition == CREATE_NEW || nCreationDisposition == OPEN_ALWAYS)
		{
			// We create folder
			if (!CreateDirectoryW(sTargetPath, &tSecurityAttributes))
			{
				nErrorCode = GetLastError();

				// Fail to create folder for OPEN_ALWAYS is not an error
				if (nErrorCode != ERROR_ALREADY_EXISTS || nCreationDisposition == CREATE_NEW)
				{
					nStatusCode = GetNtStatusByWin32ErrorCode(nErrorCode);
				}
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
		// It is a create file request

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

	if (pFileSecurity)
	{
		DestroyPrivateObjectSecurity(reinterpret_cast<PSECURITY_DESCRIPTOR*>(&pFileSecurity));
	}
	return nStatusCode;
}
NTSTATUS KxVFSMirror::OnCloseFile(DOKAN_CLOSE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(pMirrorHandle->Lock);

			pMirrorHandle->IsClosed = true;
			if (pMirrorHandle->FileHandle && pMirrorHandle->FileHandle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pMirrorHandle->FileHandle);
				pMirrorHandle->FileHandle = NULL;

				KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
				CheckDeleteOnClose(pEventInfo->DokanFileInfo, sTargetPath);
			}
		}

		pEventInfo->DokanFileInfo->Context = NULL;
		PushMirrorFileHandle(pMirrorHandle);
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnCleanUp(DOKAN_CLEANUP_EVENT* pEventInfo)
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
			pMirrorHandle->IsCleanedUp = true;

			KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
			CheckDeleteOnClose(pEventInfo->DokanFileInfo, sTargetPath);
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnMoveFile(DOKAN_MOVE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		//KxDynamicString sTargetPathOld = GetTargetPath(pEventInfo->FileName);
		KxDynamicString sTargetPathNew = GetTargetPath(pEventInfo->NewFileName);

		// The FILE_RENAME_INFO struct has space for one WCHAR for the name at
		// the end, so that accounts for the null terminator
		DWORD nBufferSize = (DWORD)(sizeof(FILE_RENAME_INFO) + sTargetPathNew.length() * sizeof(sTargetPathNew[0]));
		PFILE_RENAME_INFO pRenameInfo = (PFILE_RENAME_INFO)malloc(nBufferSize);

		if (!pRenameInfo)
		{
			return STATUS_BUFFER_OVERFLOW;
		}
		ZeroMemory(pRenameInfo, nBufferSize);

		pRenameInfo->ReplaceIfExists = pEventInfo->ReplaceIfExists ? TRUE : FALSE; // Some warning about converting BOOL to BOOLEAN
		pRenameInfo->RootDirectory = NULL; // Hope it is never needed, shouldn't be

		pRenameInfo->FileNameLength = (DWORD)sTargetPathNew.length() * sizeof(sTargetPathNew[0]); // They want length in bytes
		wcscpy_s(pRenameInfo->FileName, sTargetPathNew.length() + 1, sTargetPathNew);

		BOOL bResult = SetFileInformationByHandle(pMirrorHandle->FileHandle, FileRenameInfo, pRenameInfo, nBufferSize);
		free(pRenameInfo);

		if (bResult)
		{
			return STATUS_SUCCESS;
		}
		else
		{
			return GetNtStatusByWin32LastErrorCode();
		}
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		BY_HANDLE_FILE_INFORMATION tFileInfo = {0};
		if (!GetFileInformationByHandle(pMirrorHandle->FileHandle, &tFileInfo))
		{
			return DokanNtStatusFromWin32(GetLastError());
		}
		if ((tFileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY)
		{
			return STATUS_CANNOT_DELETE;
		}

		if (pEventInfo->DokanFileInfo->IsDirectory)
		{
			KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
			return CanDeleteDirectory(sTargetPath);
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}

NTSTATUS KxVFSMirror::OnLockFile(DOKAN_LOCK_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		LARGE_INTEGER nLength;
		nLength.QuadPart = pEventInfo->Length;

		LARGE_INTEGER nOffset;
		nOffset.QuadPart = pEventInfo->ByteOffset;

		if (!LockFile(pMirrorHandle->FileHandle, nOffset.LowPart, nOffset.HighPart, nLength.LowPart, nLength.HighPart))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnUnlockFile(DOKAN_UNLOCK_FILE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		LARGE_INTEGER nLength;
		nLength.QuadPart = pEventInfo->Length;

		LARGE_INTEGER nOffset;
		nOffset.QuadPart = pEventInfo->ByteOffset;

		if (!UnlockFile(pMirrorHandle->FileHandle, nOffset.LowPart, nOffset.HighPart, nLength.LowPart, nLength.HighPart))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* pEventInfo)
{
	return STATUS_NOT_IMPLEMENTED;

	#if 0
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		PSECURITY_DESCRIPTOR pTempSecurityDescriptor = NULL;

		// GetSecurityInfo() is not thread safe so we use a critical section here to synchronize
		DWORD nErrorCode = ERROR_SUCCESS;
		{
			KxVFSCriticalSectionLocker lock(pMirrorHandle->Lock);

			if (pMirrorHandle->IsClosed)
			{
				nErrorCode = ERROR_INVALID_HANDLE;
			}
			else if (pMirrorHandle->IsCleanedUp)
			{
				KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
				nErrorCode = GetNamedSecurityInfoW(sTargetPath, SE_FILE_OBJECT, pEventInfo->SecurityInformation, NULL, NULL, NULL, NULL, &pTempSecurityDescriptor);
			}
			else
			{
				nErrorCode = GetSecurityInfo(pMirrorHandle->FileHandle, SE_FILE_OBJECT, pEventInfo->SecurityInformation, NULL, NULL, NULL, NULL, &pTempSecurityDescriptor);
			}
		}

		if (nErrorCode != ERROR_SUCCESS)
		{
			return GetNtStatusByWin32ErrorCode(nErrorCode);
		}

		SECURITY_DESCRIPTOR_CONTROL nControl = 0;
		DWORD nRevision;
		if (!GetSecurityDescriptorControl(pTempSecurityDescriptor, &nControl, &nRevision))
		{
			//DbgPrint(L"  GetSecurityDescriptorControl error: %d\n", GetLastError());
		}

		if (!(nControl & SE_SELF_RELATIVE))
		{
			if (!MakeSelfRelativeSD(pTempSecurityDescriptor, pEventInfo->SecurityDescriptor, &pEventInfo->LengthNeeded))
			{
				nErrorCode = GetLastError();
				LocalFree(pTempSecurityDescriptor);

				if (nErrorCode == ERROR_INSUFFICIENT_BUFFER)
				{
					return STATUS_BUFFER_OVERFLOW;
				}
				return GetNtStatusByWin32ErrorCode(nErrorCode);
			}
		}
		else
		{
			pEventInfo->LengthNeeded = GetSecurityDescriptorLength(pTempSecurityDescriptor);
			if (pEventInfo->LengthNeeded > pEventInfo->SecurityDescriptorSize)
			{
				LocalFree(pTempSecurityDescriptor);
				return STATUS_BUFFER_OVERFLOW;
			}
			memcpy_s(pEventInfo->SecurityDescriptor, pEventInfo->SecurityDescriptorSize, pTempSecurityDescriptor, pEventInfo->LengthNeeded);
		}
		LocalFree(pTempSecurityDescriptor);

		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
	#endif
}
NTSTATUS KxVFSMirror::OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* pEventInfo)
{
	return STATUS_NOT_IMPLEMENTED;

	#if 0
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		// SecurityDescriptor must be 4-byte aligned
		//assert(((size_t)EventInfo->SecurityDescriptor & 3) == 0);

		BOOL bSetSecurity = FALSE;

		{
			KxVFSCriticalSectionLocker lock(pMirrorHandle->Lock);

			if (pMirrorHandle->IsClosed)
			{
				bSetSecurity = FALSE;
				SetLastError(ERROR_INVALID_HANDLE);
			}
			else if (pMirrorHandle->IsCleanedUp)
			{
				// I wonder why it was commented out
				#if 0
				PSID owner = NULL;
				PSID group = NULL;
				PACL dacl = NULL;
				PACL sacl = NULL;
				BOOL ownerDefault = FALSE;

				if (EventInfo->SecurityInformation & (OWNER_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
				{
					GetSecurityDescriptorOwner(EventInfo->SecurityDescriptor, &owner, &ownerDefault);
				}

				if (EventInfo->SecurityInformation & (GROUP_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
				{
					GetSecurityDescriptorGroup(EventInfo->SecurityDescriptor, &group, &ownerDefault);
				}

				if (EventInfo->SecurityInformation & (DACL_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
				{
					BOOL hasDacl = FALSE;
					GetSecurityDescriptorDacl(EventInfo->SecurityDescriptor, &hasDacl, &dacl, &ownerDefault);
				}

				if (EventInfo->SecurityInformation &
					(GROUP_SECURITY_INFORMATION
					 | BACKUP_SECURITY_INFORMATION
					 | LABEL_SECURITY_INFORMATION
					 | SACL_SECURITY_INFORMATION
					 | SCOPE_SECURITY_INFORMATION
					 | ATTRIBUTE_SECURITY_INFORMATION))
				{
					BOOL hasSacl = FALSE;
					GetSecurityDescriptorSacl(EventInfo->SecurityDescriptor, &hasSacl, &sacl, &ownerDefault);
				}
				setSecurity = SetNamedSecurityInfoW(filePath, SE_FILE_OBJECT, EventInfo->SecurityInformation, owner, group, dacl, sacl) == ERROR_SUCCESS;
				#endif

				KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);
				bSetSecurity = SetFileSecurityW(sTargetPath, pEventInfo->SecurityInformation, pEventInfo->SecurityDescriptor);
			}
			else
			{
				// For some reason this appears to be only variant of SetSecurity that works without returning an ERROR_ACCESS_DENIED
				bSetSecurity = SetUserObjectSecurity(pMirrorHandle->FileHandle, &pEventInfo->SecurityInformation, pEventInfo->SecurityDescriptor);
			}
		}

		if (!bSetSecurity)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
	#endif
}

NTSTATUS KxVFSMirror::OnReadFile(DOKAN_READ_FILE_EVENT* pEventInfo)
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
NTSTATUS KxVFSMirror::OnWriteFile(DOKAN_WRITE_FILE_EVENT* pEventInfo)
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
NTSTATUS KxVFSMirror::OnFlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (!pMirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	if (FlushFileBuffers(pMirrorHandle->FileHandle))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}
NTSTATUS KxVFSMirror::OnSetEndOfFile(DOKAN_SET_EOF_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		LARGE_INTEGER tOffset;
		tOffset.QuadPart = pEventInfo->Length;

		if (!SetFilePointerEx(pMirrorHandle->FileHandle, tOffset, NULL, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		if (!SetEndOfFile(pMirrorHandle->FileHandle))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnSetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* nMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (nMirrorHandle)
	{
		LARGE_INTEGER nFileSize;
		if (GetFileSizeEx(nMirrorHandle->FileHandle, &nFileSize))
		{
			if (pEventInfo->Length < nFileSize.QuadPart)
			{
				nFileSize.QuadPart = pEventInfo->Length;

				if (!SetFilePointerEx(nMirrorHandle->FileHandle, nFileSize, NULL, FILE_BEGIN))
				{
					return GetNtStatusByWin32LastErrorCode();
				}
				if (!SetEndOfFile(nMirrorHandle->FileHandle))
				{
					return GetNtStatusByWin32LastErrorCode();
				}
			}
		}
		else
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnGetFileInfo(DOKAN_GET_FILE_INFO_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		if (!GetFileInformationByHandle(pMirrorHandle->FileHandle, &pEventInfo->FileHandleInfo))
		{
			KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);

			// FileName is a root directory
			// in this case, FindFirstFile can't get directory information
			if (wcslen(pEventInfo->FileName) == 1)
			{
				pEventInfo->FileHandleInfo.dwFileAttributes = ::GetFileAttributesW(sTargetPath);
			}
			else
			{
				WIN32_FIND_DATAW tFindData = {0};
				HANDLE hFile = FindFirstFileW(sTargetPath, &tFindData);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					return GetNtStatusByWin32LastErrorCode();
				}

				pEventInfo->FileHandleInfo.dwFileAttributes = tFindData.dwFileAttributes;
				pEventInfo->FileHandleInfo.ftCreationTime = tFindData.ftCreationTime;
				pEventInfo->FileHandleInfo.ftLastAccessTime = tFindData.ftLastAccessTime;
				pEventInfo->FileHandleInfo.ftLastWriteTime = tFindData.ftLastWriteTime;
				pEventInfo->FileHandleInfo.nFileSizeHigh = tFindData.nFileSizeHigh;
				pEventInfo->FileHandleInfo.nFileSizeLow = tFindData.nFileSizeLow;
				FindClose(hFile);
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnSetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* pEventInfo)
{
	KxVFSMirror_FileHandle* pMirrorHandle = (KxVFSMirror_FileHandle*)pEventInfo->DokanFileInfo->Context;
	if (pMirrorHandle)
	{
		if (!SetFileInformationByHandle(pMirrorHandle->FileHandle, FileBasicInfo, pEventInfo->Info, (DWORD)sizeof(FILE_BASIC_INFORMATION)))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}

NTSTATUS KxVFSMirror::OnFindFiles(DOKAN_FIND_FILES_EVENT* pEventInfo)
{
	KxDynamicString sTargetPath = GetTargetPath(pEventInfo->PathName);
	size_t sTargetPathLength = sTargetPath.length();

	if (sTargetPath[sTargetPathLength - 1] != L'\\')
	{
		sTargetPath[sTargetPathLength++] = L'\\';
	}
	sTargetPath[sTargetPathLength] = L'*';
	sTargetPath[sTargetPathLength + 1] = L'\0';

	WIN32_FIND_DATAW tFindData = {0};
	HANDLE hFind = FindFirstFileW(sTargetPath, &tFindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	// Root folder does not have . and .. folder - we remove them
	bool bRootFolder = (wcscmp(pEventInfo->PathName, L"\\") == 0);

	size_t nCount = 0;
	do
	{
		if (!bRootFolder || (wcscmp(tFindData.cFileName, L".") != 0 && wcscmp(tFindData.cFileName, L"..") != 0))
		{
			pEventInfo->FillFindData(pEventInfo, &tFindData);
		}
		nCount++;
	}
	while (FindNextFileW(hFind, &tFindData) != 0);

	DWORD nErrorCode = GetLastError();
	FindClose(hFind);

	if (nErrorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnFindStreams(DOKAN_FIND_STREAMS_EVENT* pEventInfo)
{
	KxDynamicString sTargetPath = GetTargetPath(pEventInfo->FileName);

	WIN32_FIND_STREAM_DATA tFindData;
	HANDLE hFind = FindFirstStreamW(sTargetPath, FindStreamInfoStandard, &tFindData, 0);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	DWORD nErrorCode = 0;
	size_t nCount = 0;

	DOKAN_STREAM_FIND_RESULT nFindResult = DOKAN_STREAM_BUFFER_CONTINUE;
	if ((nFindResult = pEventInfo->FillFindStreamData(pEventInfo, &tFindData)) == DOKAN_STREAM_BUFFER_CONTINUE)
	{
		nCount++;
		while (FindNextStreamW(hFind, &tFindData) != 0 && (nFindResult = pEventInfo->FillFindStreamData(pEventInfo, &tFindData)) == DOKAN_STREAM_BUFFER_CONTINUE)
		{
			nCount++;
		}
	}

	nErrorCode = GetLastError();
	FindClose(hFind);

	if (nFindResult == DOKAN_STREAM_BUFFER_FULL)
	{
		// FindStreams returned 'nCount' entries in 'sInSourcePath' with STATUS_BUFFER_OVERFLOW
		// https://msdn.microsoft.com/en-us/library/windows/hardware/ff540364(v=vs.85).aspx
		return STATUS_BUFFER_OVERFLOW;
	}

	if (nErrorCode != ERROR_HANDLE_EOF)
	{
		return GetNtStatusByWin32ErrorCode(nErrorCode);
	}
	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
void KxVFSMirror::FreeMirrorFileHandle(KxVFSMirror_FileHandle* pFileHandle)
{
	if (pFileHandle)
	{
		#if KxVFS_USE_ASYNC_IO
		if (pFileHandle->IoCompletion)
		{
			{
				KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
				if (m_ThreadPoolCleanupGroup)
				{
					CloseThreadpoolIo(pFileHandle->IoCompletion);
				}
			}
			pFileHandle->IoCompletion = NULL;
		}
		#endif

		//DeleteCriticalSection(&pFileHandle->Lock);
		//free(pFileHandle);
		delete pFileHandle;
	}
}
void KxVFSMirror::PushMirrorFileHandle(KxVFSMirror_FileHandle* pFileHandle)
{
	pFileHandle->self = this;

	#if KxVFS_USE_ASYNC_IO
	if (pFileHandle->IoCompletion)
	{
		{
			KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
			if (m_ThreadPoolCleanupGroup)
			{
				CloseThreadpoolIo(pFileHandle->IoCompletion);
			}
		}
		pFileHandle->IoCompletion = NULL;
	}
	#endif

	KxVFSCriticalSectionLocker lock(m_FileHandlePoolCS);
	{
		DokanVector_PushBack(&m_FileHandlePool, &pFileHandle);
	}
}
KxVFSMirror_FileHandle* KxVFSMirror::PopMirrorFileHandle(HANDLE hActualFileHandle)
{
	if (hActualFileHandle == NULL || hActualFileHandle == INVALID_HANDLE_VALUE || InterlockedAdd(&m_IsUnmounted, 0) != FALSE)
	{
		return NULL;
	}

	KxVFSMirror_FileHandle* pMirrorHandle = NULL;
	KxVFSCriticalSectionLocker lock(m_FileHandlePoolCS);
	{
		if (DokanVector_GetCount(&m_FileHandlePool) > 0)
		{
			pMirrorHandle = *(KxVFSMirror_FileHandle**)DokanVector_GetLastItem(&m_FileHandlePool);
			DokanVector_PopBack(&m_FileHandlePool);
		}
	}

	if (!pMirrorHandle)
	{
		pMirrorHandle = new KxVFSMirror_FileHandle(this);
	}

	pMirrorHandle->FileHandle = hActualFileHandle;
	pMirrorHandle->IsCleanedUp = false;
	pMirrorHandle->IsClosed = false;

	#if KxVFS_USE_ASYNC_IO
	{
		KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
		if (m_ThreadPoolCleanupGroup)
		{
			pMirrorHandle->IoCompletion = CreateThreadpoolIo(hActualFileHandle, MirrorIoCallback, pMirrorHandle, &m_ThreadPoolEnvironment);
		}
	}

	if (!pMirrorHandle->IoCompletion)
	{
		PushMirrorFileHandle(pMirrorHandle);
		pMirrorHandle = NULL;
	}
	#endif
	return pMirrorHandle;
}
void KxVFSMirror::GetMirrorFileHandleState(KxVFSMirror_FileHandle* pFileHandle, bool* pIsCleanedUp, bool* bIsClosed) const
{
	if (pFileHandle && (pIsCleanedUp || bIsClosed))
	{
		KxVFSCriticalSectionLocker lock(pFileHandle->Lock);

		if (pIsCleanedUp)
		{
			*pIsCleanedUp = pFileHandle->IsCleanedUp;
		}
		if (bIsClosed)
		{
			*bIsClosed = pFileHandle->IsClosed;
		}
	}
}

BOOL KxVFSMirror::InitializeMirrorFileHandles()
{
	return DokanVector_StackAlloc(&m_FileHandlePool, sizeof(KxVFSMirror_FileHandle*));
}
void KxVFSMirror::CleanupMirrorFileHandles()
{
	KxVFSCriticalSectionLocker lock(m_FileHandlePoolCS);

	for (size_t i = 0; i < DokanVector_GetCount(&m_FileHandlePool); ++i)
	{
		FreeMirrorFileHandle(*(KxVFSMirror_FileHandle**)DokanVector_GetItem(&m_FileHandlePool, i));
	}
	DokanVector_Free(&m_FileHandlePool);
}

#if KxVFS_USE_ASYNC_IO
void KxVFSMirror::FreeMirrorOverlapped(KxVFSMirror_Overlapped* pOverlapped) const
{
	delete pOverlapped;
}
void KxVFSMirror::PushMirrorOverlapped(KxVFSMirror_Overlapped* pOverlapped)
{
	KxVFSCriticalSectionLocker lock(m_OverlappedPoolCS);
	DokanVector_PushBack(&m_OverlappedPool, &pOverlapped);
}
KxVFSMirror_Overlapped* KxVFSMirror::PopMirrorOverlapped()
{
	KxVFSMirror_Overlapped* pOverlapped = NULL;
	{
		KxVFSCriticalSectionLocker lock(m_OverlappedPoolCS);
		if (DokanVector_GetCount(&m_OverlappedPool) > 0)
		{
			pOverlapped = *(KxVFSMirror_Overlapped**)DokanVector_GetLastItem(&m_OverlappedPool);
			DokanVector_PopBack(&m_OverlappedPool);
		}
	}

	if (!pOverlapped)
	{
		pOverlapped = new KxVFSMirror_Overlapped();
	}
	return pOverlapped;
}

BOOL KxVFSMirror::InitializeAsyncIO()
{
	m_ThreadPool = DokanGetThreadPool();
	if (!m_ThreadPool)
	{
		return FALSE;
	}

	m_ThreadPoolCleanupGroup = CreateThreadpoolCleanupGroup();
	if (!m_ThreadPoolCleanupGroup)
	{
		return FALSE;
	}

	InitializeThreadpoolEnvironment(&m_ThreadPoolEnvironment);

	SetThreadpoolCallbackPool(&m_ThreadPoolEnvironment, m_ThreadPool);
	SetThreadpoolCallbackCleanupGroup(&m_ThreadPoolEnvironment, m_ThreadPoolCleanupGroup, NULL);

	DokanVector_StackAlloc(&m_OverlappedPool, sizeof(KxVFSMirror_Overlapped*));
	return TRUE;
}
void KxVFSMirror::CleanupPendingAsyncIO()
{
	KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);

	if (m_ThreadPoolCleanupGroup)
	{
		CloseThreadpoolCleanupGroupMembers(m_ThreadPoolCleanupGroup, FALSE, NULL);
		CloseThreadpoolCleanupGroup(m_ThreadPoolCleanupGroup);
		m_ThreadPoolCleanupGroup = NULL;

		DestroyThreadpoolEnvironment(&m_ThreadPoolEnvironment);
	}
}
void KxVFSMirror::CleanupAsyncIO()
{
	CleanupPendingAsyncIO();

	{
		KxVFSCriticalSectionLocker lock(m_OverlappedPoolCS);
		for (size_t i = 0; i < DokanVector_GetCount(&m_OverlappedPool); ++i)
		{
			FreeMirrorOverlapped(*(KxVFSMirror_Overlapped**)DokanVector_GetItem(&m_OverlappedPool, i));
		}
		DokanVector_Free(&m_OverlappedPool);
	}
}
void CALLBACK KxVFSMirror::MirrorIoCallback
(
	PTP_CALLBACK_INSTANCE Instance,
	PVOID Context,
	PVOID Overlapped,
	ULONG IoResult,
	ULONG_PTR NumberOfBytesTransferred,
	PTP_IO Io
)
{
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(Io);

	KxVFSMirror_FileHandle* pHandle = reinterpret_cast<KxVFSMirror_FileHandle*>(Context);
	KxVFSMirror_Overlapped* pOverlapped = (KxVFSMirror_Overlapped*)Overlapped;
	DOKAN_READ_FILE_EVENT* pReadFile = NULL;
	DOKAN_WRITE_FILE_EVENT* pWriteFile = NULL;

	switch (pOverlapped->IoType)
	{
		case MIRROR_IOTYPE_READ:
		{
			pReadFile = (DOKAN_READ_FILE_EVENT*)pOverlapped->Context;
			pReadFile->NumberOfBytesRead = (DWORD)NumberOfBytesTransferred;
			DokanEndDispatchRead(pReadFile, DokanNtStatusFromWin32(IoResult));
			break;
		}
		case MIRROR_IOTYPE_WRITE:
		{
			pWriteFile = (DOKAN_WRITE_FILE_EVENT*)pOverlapped->Context;
			pWriteFile->NumberOfBytesWritten = (DWORD)NumberOfBytesTransferred;
			DokanEndDispatchWrite(pWriteFile, DokanNtStatusFromWin32(IoResult));
			break;
		}
	};
	pHandle->self->PushMirrorOverlapped(pOverlapped);
}
#endif
