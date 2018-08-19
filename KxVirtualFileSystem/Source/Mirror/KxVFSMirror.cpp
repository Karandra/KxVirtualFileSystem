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
DWORD KxVFSMirror::GetParentSecurity(const WCHAR* filePath, PSECURITY_DESCRIPTOR* parentSecurity) const
{
	int lastPathSeparator = -1;
	for (int i = 0; i < MaxPathLength && filePath[i]; ++i)
	{
		if (filePath[i] == '\\')
		{
			lastPathSeparator = i;
		}
	}
	if (lastPathSeparator == -1)
	{
		return ERROR_PATH_NOT_FOUND;
	}

	WCHAR parentPath[MaxPathLength];
	memcpy_s(parentPath, MaxPathLength * sizeof(WCHAR), filePath, lastPathSeparator * sizeof(WCHAR));
	parentPath[lastPathSeparator] = 0;

	// Must LocalFree() parentSecurity
	return GetNamedSecurityInfoW
	(
		parentPath,
		SE_FILE_OBJECT,
		BACKUP_SECURITY_INFORMATION, // give us everything
		NULL,
		NULL,
		NULL,
		NULL,
		parentSecurity
	);
}
DWORD KxVFSMirror::CreateNewSecurity(DOKAN_CREATE_FILE_EVENT* eventInfo, const WCHAR* filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const
{
	if (!eventInfo || !filePath || !requestedSecurity || !newSecurity)
	{
		return ERROR_INVALID_PARAMETER;
	}

	int errorCode = ERROR_SUCCESS;
	PSECURITY_DESCRIPTOR parentDescriptor = NULL;
	if ((errorCode = GetParentSecurity(filePath, &parentDescriptor)) == ERROR_SUCCESS)
	{
		static GENERIC_MAPPING g_GenericMapping =
		{
			FILE_GENERIC_READ,
			FILE_GENERIC_WRITE,
			FILE_GENERIC_EXECUTE,
			FILE_ALL_ACCESS
		};

		HANDLE aaccessTokenHandle = DokanOpenRequestorToken(eventInfo->DokanFileInfo);
		if (aaccessTokenHandle && aaccessTokenHandle != INVALID_HANDLE_VALUE)
		{
			if (!CreatePrivateObjectSecurity(parentDescriptor,
											 eventInfo->SecurityContext.AccessState.SecurityDescriptor,
											 newSecurity,
											 eventInfo->DokanFileInfo->IsDirectory,
											 aaccessTokenHandle,
											 &g_GenericMapping))
			{

				errorCode = GetLastError();
			}
			CloseHandle(aaccessTokenHandle);
		}
		else
		{
			errorCode = GetLastError();
		}
		LocalFree(parentDescriptor);
	}
	return errorCode;
}

bool KxVFSMirror::CheckDeleteOnClose(PDOKAN_FILE_INFO fileInfo, const WCHAR* filePath) const
{
	if (fileInfo->DeleteOnClose)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), filePath);

		// Should already be deleted by CloseHandle
		// if open with FILE_FLAG_DELETE_ON_CLOSE
		if (fileInfo->IsDirectory)
		{
			RemoveDirectoryW(filePath);
		}
		else
		{
			DeleteFileW(filePath);
		}
		return true;
	}
	return false;
}
NTSTATUS KxVFSMirror::CanDeleteDirectory(const WCHAR* filePath) const
{
	KxDynamicString filePathCopy = filePath;

	size_t nFilePathLength = filePathCopy.length();
	if (filePathCopy[nFilePathLength - 1] != L'\\')
	{
		filePathCopy[nFilePathLength++] = L'\\';
	}

	filePathCopy[nFilePathLength] = L'*';
	filePathCopy[nFilePathLength + 1] = L'\0';

	WIN32_FIND_DATAW findData = {0};
	HANDLE findHandle = FindFirstFileW(filePathCopy, &findData);
	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	do
	{
		if (wcscmp(findData.cFileName, L"..") != 0 && wcscmp(findData.cFileName, L".") != 0)
		{
			FindClose(findHandle);
			return STATUS_DIRECTORY_NOT_EMPTY;
		}
	}
	while (FindNextFileW(findHandle, &findData) != 0);

	DWORD errorCode = GetLastError();
	if (errorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32ErrorCode(errorCode);
	}

	FindClose(findHandle);
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::ReadFileSynchronous(DOKAN_READ_FILE_EVENT* eventInfo, HANDLE fileHandle) const
{
	LARGE_INTEGER distanceToMove;
	distanceToMove.QuadPart = eventInfo->Offset;

	if (!SetFilePointerEx(fileHandle, distanceToMove, NULL, FILE_BEGIN))
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	if (ReadFile(fileHandle, eventInfo->Buffer, eventInfo->NumberOfBytesToRead, &eventInfo->NumberOfBytesRead, NULL))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}
NTSTATUS KxVFSMirror::WriteFileSynchronous(DOKAN_WRITE_FILE_EVENT* eventInfo, HANDLE fileHandle, UINT64 fileSize) const
{
	LARGE_INTEGER distanceToMove;

	if (eventInfo->DokanFileInfo->WriteToEndOfFile)
	{
		LARGE_INTEGER pos;
		pos.QuadPart = 0;

		if (!SetFilePointerEx(fileHandle, pos, NULL, FILE_END))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
	}
	else
	{
		// Paging IO cannot write after allocate file size.
		if (eventInfo->DokanFileInfo->PagingIo)
		{
			if ((UINT64)eventInfo->Offset >= fileSize)
			{
				eventInfo->NumberOfBytesWritten = 0;
				return STATUS_SUCCESS;
			}

			// WFT is happening here?
			if (((UINT64)eventInfo->Offset + eventInfo->NumberOfBytesToWrite) > fileSize)
			{
				UINT64 bytes = fileSize - eventInfo->Offset;
				if (bytes >> 32)
				{
					eventInfo->NumberOfBytesToWrite = (DWORD)(bytes & 0xFFFFFFFFUL);
				}
				else {

					eventInfo->NumberOfBytesToWrite = (DWORD)bytes;
				}
			}
		}

		if ((UINT64)eventInfo->Offset > fileSize)
		{
			// In the mirror sample helperZeroFileData is not necessary. NTFS will zero a hole.
			// But if user's file system is different from NTFS (or other Windows
			// file systems) then  users will have to zero the hole themselves.

			// If only Dokan devs can explain more clearly what they are talking about
		}

		distanceToMove.QuadPart = eventInfo->Offset;
		if (!SetFilePointerEx(fileHandle, distanceToMove, NULL, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
	}

	if (WriteFile(fileHandle, eventInfo->Buffer, eventInfo->NumberOfBytesToWrite, &eventInfo->NumberOfBytesWritten, NULL))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}

KxDynamicString KxVFSMirror::GetTargetPath(const WCHAR* requestedPath)
{
	KxDynamicString targetPath(L"\\\\?\\");
	targetPath.append(m_Source);
	targetPath.append(requestedPath);
	return targetPath;
}

//////////////////////////////////////////////////////////////////////////
KxVFSMirror::KxVFSMirror(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* source, ULONG falgs, ULONG requestTimeout)
	:KxVFSBase(vfsService, mountPoint, falgs, requestTimeout), m_Source(source)
{
}
KxVFSMirror::~KxVFSMirror()
{
}

bool KxVFSMirror::SetSource(const WCHAR* source)
{
	if (!IsMounted())
	{
		m_Source = source;
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
NTSTATUS KxVFSMirror::OnMount(DOKAN_MOUNTED_INFO* eventInfo)
{
	return STATUS_NOT_SUPPORTED;
}
NTSTATUS KxVFSMirror::OnUnMount(DOKAN_UNMOUNTED_INFO* eventInfo)
{
	CleanupMirrorFileHandles();
	#if KxVFS_USE_ASYNC_IO
	CleanupAsyncIO();
	#endif

	return STATUS_NOT_SUPPORTED;
}

NTSTATUS KxVFSMirror::OnGetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* eventInfo)
{
	if (!GetDiskFreeSpaceExW(GetSource(), (ULARGE_INTEGER*)&eventInfo->FreeBytesAvailable, (ULARGE_INTEGER*)&eventInfo->TotalNumberOfBytes, (ULARGE_INTEGER*)&eventInfo->TotalNumberOfFreeBytes))
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnGetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* eventInfo)
{
	eventInfo->VolumeInfo->VolumeSerialNumber = GetVolumeSerialNumber();
	eventInfo->VolumeInfo->VolumeLabelLength = WriteString(GetVolumeName(), eventInfo->VolumeInfo->VolumeLabel, eventInfo->MaxLabelLengthInChars);
	eventInfo->VolumeInfo->SupportsObjects = FALSE;

	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnGetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* eventInfo)
{
	LPCWSTR fileSystemName = GetVolumeFileSystemName();
	size_t maxFileSystemNameLengthInBytes = eventInfo->MaxFileSystemNameLengthInChars * sizeof(WCHAR);

	eventInfo->Attributes->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH|FILE_CASE_PRESERVED_NAMES|FILE_SUPPORTS_REMOTE_STORAGE|FILE_UNICODE_ON_DISK|FILE_PERSISTENT_ACLS|FILE_NAMED_STREAMS;
	eventInfo->Attributes->MaximumComponentNameLength = 256; // Not 255, this means Dokan doesn't support long file names?

	WCHAR volumeRoot[4];
	volumeRoot[0] = GetSource()[0];
	volumeRoot[1] = ':';
	volumeRoot[2] = '\\';
	volumeRoot[3] = '\0';

	DWORD fileSystemFlags = 0;
	DWORD maximumComponentLength = 0;

	WCHAR fileSystemNameBuffer[255] = {0};
	if (GetVolumeInformationW(volumeRoot, NULL, 0, NULL, &maximumComponentLength, &fileSystemFlags, fileSystemNameBuffer, ARRAYSIZE(fileSystemNameBuffer)))
	{
		eventInfo->Attributes->MaximumComponentNameLength = maximumComponentLength;
		eventInfo->Attributes->FileSystemAttributes &= fileSystemFlags;

		fileSystemName = fileSystemNameBuffer;
	}
	eventInfo->Attributes->FileSystemNameLength = WriteString(fileSystemName, eventInfo->Attributes->FileSystemName, eventInfo->MaxFileSystemNameLengthInChars);

	return STATUS_SUCCESS;
}

NTSTATUS KxVFSMirror::OnCreateFile(DOKAN_CREATE_FILE_EVENT* eventInfo)
{
	DWORD errorCode = 0;
	NTSTATUS statusCode = STATUS_SUCCESS;
	KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);

	DWORD fileAttributesAndFlags = 0;
	DWORD creationDisposition = 0;
	ACCESS_MASK genericDesiredAccess = DokanMapStandardToGenericAccess(eventInfo->DesiredAccess);
	DokanMapKernelToUserCreateFileFlags(eventInfo, &fileAttributesAndFlags, &creationDisposition);

	// When filePath is a directory, needs to change the flag so that the file can be opened.
	DWORD fileAttributes = ::GetFileAttributesW(targetPath);
	if (fileAttributes != INVALID_FILE_ATTRIBUTES)
	{
		if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!(eventInfo->CreateOptions & FILE_NON_DIRECTORY_FILE))
			{
				// Needed by FindFirstFile to list files in it
				// TODO: use ReOpenFile in MirrorFindFiles to set share read temporary
				eventInfo->DokanFileInfo->IsDirectory = TRUE;
				eventInfo->ShareAccess |= FILE_SHARE_READ;
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
	if (wcscmp(eventInfo->FileName, L"\\") != 0 && wcscmp(eventInfo->FileName, L"/") != 0	&& creationDisposition != OPEN_EXISTING && creationDisposition != TRUNCATE_EXISTING)
	{
		// We only need security information if there's a possibility a new file could be created
		CreateNewSecurity(eventInfo, targetPath, eventInfo->SecurityContext.AccessState.SecurityDescriptor, reinterpret_cast<PSECURITY_DESCRIPTOR*>(&fileSecurity));
	}

	SECURITY_ATTRIBUTES securityAttributes;
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = fileSecurity;
	securityAttributes.bInheritHandle = FALSE;

	if (eventInfo->DokanFileInfo->IsDirectory)
	{
		// It is a create directory request
		if (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS)
		{
			// We create folder
			if (!CreateDirectoryW(targetPath, &securityAttributes))
			{
				errorCode = GetLastError();

				// Fail to create folder for OPEN_ALWAYS is not an error
				if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CREATE_NEW)
				{
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
				}
			}
		}

		if (statusCode == STATUS_SUCCESS)
		{
			// Check first if we're trying to open a file as a directory.
			if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (eventInfo->CreateOptions & FILE_DIRECTORY_FILE))
			{
				return STATUS_NOT_A_DIRECTORY;
			}

			// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
			KxVFSFileHandle fileHandle = CreateFileW
			(
				targetPath,
				genericDesiredAccess,
				eventInfo->ShareAccess,
				&securityAttributes,
				OPEN_EXISTING,
				fileAttributesAndFlags|FILE_FLAG_BACKUP_SEMANTICS,
				NULL
			);

			if (fileHandle.IsOK())
			{
				KxVFSMirror_FileHandle* mirrorHandle = PopMirrorFileHandle(fileHandle);
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

				// Save the file handle in Context
				eventInfo->DokanFileInfo->Context = (ULONG64)mirrorHandle;

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
		// It is a create file request

		// Cannot overwrite a hidden or system file if flag not set
		if (fileAttributes != INVALID_FILE_ATTRIBUTES && ((!(fileAttributesAndFlags & FILE_ATTRIBUTE_HIDDEN) &&
			(fileAttributes & FILE_ATTRIBUTE_HIDDEN)) || (!(fileAttributesAndFlags & FILE_ATTRIBUTE_SYSTEM) &&
															(fileAttributes & FILE_ATTRIBUTE_SYSTEM))) &&(eventInfo->CreateDisposition == TRUNCATE_EXISTING ||
																										   eventInfo->CreateDisposition == CREATE_ALWAYS))
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

			KxVFSFileHandle fileHandle = CreateFileW
			(
				targetPath,
				genericDesiredAccess, // GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
				eventInfo->ShareAccess,
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

				KxVFSMirror_FileHandle* mirrorHandle = PopMirrorFileHandle(fileHandle);
				if (!mirrorHandle)
				{
					SetLastError(ERROR_INTERNAL_ERROR);
					statusCode = STATUS_INTERNAL_ERROR;
				}
				else
				{
					fileHandle.Release();

					// Save the file handle in Context
					eventInfo->DokanFileInfo->Context = (ULONG64)mirrorHandle;

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

	if (fileSecurity)
	{
		DestroyPrivateObjectSecurity(reinterpret_cast<PSECURITY_DESCRIPTOR*>(&fileSecurity));
	}
	return statusCode;
}
NTSTATUS KxVFSMirror::OnCloseFile(DOKAN_CLOSE_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->Lock);

			mirrorHandle->IsClosed = true;
			if (mirrorHandle->FileHandle && mirrorHandle->FileHandle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(mirrorHandle->FileHandle);
				mirrorHandle->FileHandle = NULL;

				KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
				CheckDeleteOnClose(eventInfo->DokanFileInfo, targetPath);
			}
		}

		eventInfo->DokanFileInfo->Context = NULL;
		PushMirrorFileHandle(mirrorHandle);
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnCleanUp(DOKAN_CLEANUP_EVENT* eventInfo)
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

	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->Lock);

			CloseHandle(mirrorHandle->FileHandle);

			mirrorHandle->FileHandle = NULL;
			mirrorHandle->IsCleanedUp = true;

			KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
			CheckDeleteOnClose(eventInfo->DokanFileInfo, targetPath);
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnMoveFile(DOKAN_MOVE_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		//KxDynamicString targetPathOld = GetTargetPath(eventInfo->FileName);
		KxDynamicString targetPathNew = GetTargetPath(eventInfo->NewFileName);

		// The FILE_RENAME_INFO struct has space for one WCHAR for the name at
		// the end, so that accounts for the null terminator
		DWORD bufferSize = (DWORD)(sizeof(FILE_RENAME_INFO) + targetPathNew.length() * sizeof(targetPathNew[0]));
		PFILE_RENAME_INFO renameInfo = (PFILE_RENAME_INFO)malloc(bufferSize);

		if (!renameInfo)
		{
			return STATUS_BUFFER_OVERFLOW;
		}
		ZeroMemory(renameInfo, bufferSize);

		renameInfo->ReplaceIfExists = eventInfo->ReplaceIfExists ? TRUE : FALSE; // Some warning about converting BOOL to BOOLEAN
		renameInfo->RootDirectory = NULL; // Hope it is never needed, shouldn't be

		renameInfo->FileNameLength = (DWORD)targetPathNew.length() * sizeof(targetPathNew[0]); // They want length in bytes
		wcscpy_s(renameInfo->FileName, targetPathNew.length() + 1, targetPathNew);

		BOOL result = SetFileInformationByHandle(mirrorHandle->FileHandle, FileRenameInfo, renameInfo, bufferSize);
		free(renameInfo);

		if (result)
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
NTSTATUS KxVFSMirror::OnCanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		BY_HANDLE_FILE_INFORMATION fileInfo = {0};
		if (!GetFileInformationByHandle(mirrorHandle->FileHandle, &fileInfo))
		{
			return DokanNtStatusFromWin32(GetLastError());
		}
		if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY)
		{
			return STATUS_CANNOT_DELETE;
		}

		if (eventInfo->DokanFileInfo->IsDirectory)
		{
			KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
			return CanDeleteDirectory(targetPath);
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}

NTSTATUS KxVFSMirror::OnLockFile(DOKAN_LOCK_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		LARGE_INTEGER length;
		length.QuadPart = eventInfo->Length;

		LARGE_INTEGER offset;
		offset.QuadPart = eventInfo->ByteOffset;

		if (!LockFile(mirrorHandle->FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnUnlockFile(DOKAN_UNLOCK_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		LARGE_INTEGER length;
		length.QuadPart = eventInfo->Length;

		LARGE_INTEGER offset;
		offset.QuadPart = eventInfo->ByteOffset;

		if (!UnlockFile(mirrorHandle->FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnGetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* eventInfo)
{
	return STATUS_NOT_IMPLEMENTED;

	#if 0
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		PSECURITY_DESCRIPTOR tempSecurityDescriptor = NULL;

		// GetSecurityInfo() is not thread safe so we use a critical section here to synchronize
		DWORD errorCode = ERROR_SUCCESS;
		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->Lock);

			if (mirrorHandle->IsClosed)
			{
				errorCode = ERROR_INVALID_HANDLE;
			}
			else if (mirrorHandle->IsCleanedUp)
			{
				KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
				errorCode = GetNamedSecurityInfoW(targetPath, SE_FILE_OBJECT, eventInfo->SecurityInformation, NULL, NULL, NULL, NULL, &tempSecurityDescriptor);
			}
			else
			{
				errorCode = GetSecurityInfo(mirrorHandle->FileHandle, SE_FILE_OBJECT, eventInfo->SecurityInformation, NULL, NULL, NULL, NULL, &tempSecurityDescriptor);
			}
		}

		if (errorCode != ERROR_SUCCESS)
		{
			return GetNtStatusByWin32ErrorCode(errorCode);
		}

		SECURITY_DESCRIPTOR_CONTROL control = 0;
		DWORD nRevision;
		if (!GetSecurityDescriptorControl(tempSecurityDescriptor, &control, &nRevision))
		{
			//DbgPrint(L"  GetSecurityDescriptorControl error: %d\n", GetLastError());
		}

		if (!(control & SE_SELF_RELATIVE))
		{
			if (!MakeSelfRelativeSD(tempSecurityDescriptor, eventInfo->SecurityDescriptor, &eventInfo->LengthNeeded))
			{
				errorCode = GetLastError();
				LocalFree(tempSecurityDescriptor);

				if (errorCode == ERROR_INSUFFICIENT_BUFFER)
				{
					return STATUS_BUFFER_OVERFLOW;
				}
				return GetNtStatusByWin32ErrorCode(errorCode);
			}
		}
		else
		{
			eventInfo->LengthNeeded = GetSecurityDescriptorLength(tempSecurityDescriptor);
			if (eventInfo->LengthNeeded > eventInfo->SecurityDescriptorSize)
			{
				LocalFree(tempSecurityDescriptor);
				return STATUS_BUFFER_OVERFLOW;
			}
			memcpy_s(eventInfo->SecurityDescriptor, eventInfo->SecurityDescriptorSize, tempSecurityDescriptor, eventInfo->LengthNeeded);
		}
		LocalFree(tempSecurityDescriptor);

		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
	#endif
}
NTSTATUS KxVFSMirror::OnSetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* eventInfo)
{
	return STATUS_NOT_IMPLEMENTED;

	#if 0
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		// SecurityDescriptor must be 4-byte aligned
		//assert(((size_t)EventInfo->SecurityDescriptor & 3) == 0);

		BOOL setSecurity = FALSE;

		{
			KxVFSCriticalSectionLocker lock(mirrorHandle->Lock);

			if (mirrorHandle->IsClosed)
			{
				setSecurity = FALSE;
				SetLastError(ERROR_INVALID_HANDLE);
			}
			else if (mirrorHandle->IsCleanedUp)
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

				KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
				setSecurity = SetFileSecurityW(targetPath, eventInfo->SecurityInformation, eventInfo->SecurityDescriptor);
			}
			else
			{
				// For some reason this appears to be only variant of SetSecurity that works without returning an ERROR_ACCESS_DENIED
				setSecurity = SetUserObjectSecurity(mirrorHandle->FileHandle, &eventInfo->SecurityInformation, eventInfo->SecurityDescriptor);
			}
		}

		if (!setSecurity)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
	#endif
}

NTSTATUS KxVFSMirror::OnReadFile(DOKAN_READ_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
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
		KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
		KxVFSFileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (tempFileHandle.IsOK())
		{
			return ReadFileSynchronous(eventInfo, tempFileHandle);
		}
		return GetNtStatusByWin32LastErrorCode();
	}

	#if KxVFS_USE_ASYNC_IO
	KxVFSMirror_Overlapped* overlapped = PopMirrorOverlapped();
	if (!overlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	overlapped->InternalOverlapped.Offset = (DWORD)(eventInfo->Offset & 0xffffffff);
	overlapped->InternalOverlapped.OffsetHigh = (DWORD)((eventInfo->Offset >> 32) & 0xffffffff);
	overlapped->FileHandle = mirrorHandle;
	overlapped->Context = eventInfo;
	overlapped->IoType = MIRROR_IOTYPE_READ;

	StartThreadpoolIo(mirrorHandle->IoCompletion);
	if (!ReadFile(mirrorHandle->FileHandle, eventInfo->Buffer, eventInfo->NumberOfBytesToRead, &eventInfo->NumberOfBytesRead, (LPOVERLAPPED)overlapped))
	{
		DWORD errorCode = GetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(mirrorHandle->IoCompletion);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}

	return STATUS_PENDING;
	#else
	return ReadFileSynchronous(eventInfo, mirrorHandle->FileHandle);
	#endif
}
NTSTATUS KxVFSMirror::OnWriteFile(DOKAN_WRITE_FILE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
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

	DWORD fileSizeHigh = 0;
	DWORD fileSizeLow = 0;
	UINT64 fileSize = 0;
	if (isCleanedUp)
	{
		KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);
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

		// Need to check if its really needs to be 'mirrorHandle->FileHandle' and not 'tempFileHandle'
		return WriteFileSynchronous(eventInfo, mirrorHandle->FileHandle, fileSize);
	}

	fileSizeLow = ::GetFileSize(mirrorHandle->FileHandle, &fileSizeHigh);
	if (fileSizeLow == INVALID_FILE_SIZE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	fileSize = ((UINT64)fileSizeHigh << 32) | fileSizeLow;

	#if KxVFS_USE_ASYNC_IO

	if (eventInfo->DokanFileInfo->PagingIo)
	{
		if ((UINT64)eventInfo->Offset >= fileSize)
		{
			eventInfo->NumberOfBytesWritten = 0;
			return STATUS_SUCCESS;
		}

		if (((UINT64)eventInfo->Offset + eventInfo->NumberOfBytesToWrite) > fileSize)
		{
			UINT64 bytes = fileSize - eventInfo->Offset;
			if (bytes >> 32)
			{
				eventInfo->NumberOfBytesToWrite = (DWORD)(bytes & 0xFFFFFFFFUL);
			}
			else
			{
				eventInfo->NumberOfBytesToWrite = (DWORD)bytes;
			}
		}
	}

	KxVFSMirror_Overlapped* overlapped = PopMirrorOverlapped();
	if (!overlapped)
	{
		return STATUS_MEMORY_NOT_ALLOCATED;
	}

	overlapped->InternalOverlapped.Offset = (DWORD)(eventInfo->Offset & 0xffffffff);
	overlapped->InternalOverlapped.OffsetHigh = (DWORD)((eventInfo->Offset >> 32) & 0xffffffff);
	overlapped->FileHandle = mirrorHandle;
	overlapped->Context = eventInfo;
	overlapped->IoType = MIRROR_IOTYPE_WRITE;

	StartThreadpoolIo(mirrorHandle->IoCompletion);
	if (!WriteFile(mirrorHandle->FileHandle, eventInfo->Buffer, eventInfo->NumberOfBytesToWrite, &eventInfo->NumberOfBytesWritten, (LPOVERLAPPED)overlapped))
	{
		DWORD errorCode = GetLastError();
		if (errorCode != ERROR_IO_PENDING)
		{
			CancelThreadpoolIo(mirrorHandle->IoCompletion);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}
	return STATUS_PENDING;
	#else
	return WriteFileSynchronous(eventInfo, mirrorHandle->FileHandle, fileSize);
	#endif
}
NTSTATUS KxVFSMirror::OnFlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (!mirrorHandle)
	{
		return STATUS_FILE_CLOSED;
	}

	if (FlushFileBuffers(mirrorHandle->FileHandle))
	{
		return STATUS_SUCCESS;
	}
	return GetNtStatusByWin32LastErrorCode();
}
NTSTATUS KxVFSMirror::OnSetEndOfFile(DOKAN_SET_EOF_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		LARGE_INTEGER offset;
		offset.QuadPart = eventInfo->Length;

		if (!SetFilePointerEx(mirrorHandle->FileHandle, offset, NULL, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		if (!SetEndOfFile(mirrorHandle->FileHandle))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnSetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx(mirrorHandle->FileHandle, &fileSize))
		{
			if (eventInfo->Length < fileSize.QuadPart)
			{
				fileSize.QuadPart = eventInfo->Length;

				if (!SetFilePointerEx(mirrorHandle->FileHandle, fileSize, NULL, FILE_BEGIN))
				{
					return GetNtStatusByWin32LastErrorCode();
				}
				if (!SetEndOfFile(mirrorHandle->FileHandle))
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
NTSTATUS KxVFSMirror::OnGetFileInfo(DOKAN_GET_FILE_INFO_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		if (!GetFileInformationByHandle(mirrorHandle->FileHandle, &eventInfo->FileHandleInfo))
		{
			KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);

			// FileName is a root directory
			// in this case, FindFirstFile can't get directory information
			if (wcslen(eventInfo->FileName) == 1)
			{
				eventInfo->FileHandleInfo.dwFileAttributes = ::GetFileAttributesW(targetPath);
			}
			else
			{
				WIN32_FIND_DATAW findData = {0};
				HANDLE fileHandle = FindFirstFileW(targetPath, &findData);
				if (fileHandle == INVALID_HANDLE_VALUE)
				{
					return GetNtStatusByWin32LastErrorCode();
				}

				eventInfo->FileHandleInfo.dwFileAttributes = findData.dwFileAttributes;
				eventInfo->FileHandleInfo.ftCreationTime = findData.ftCreationTime;
				eventInfo->FileHandleInfo.ftLastAccessTime = findData.ftLastAccessTime;
				eventInfo->FileHandleInfo.ftLastWriteTime = findData.ftLastWriteTime;
				eventInfo->FileHandleInfo.nFileSizeHigh = findData.nFileSizeHigh;
				eventInfo->FileHandleInfo.nFileSizeLow = findData.nFileSizeLow;
				FindClose(fileHandle);
			}
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}
NTSTATUS KxVFSMirror::OnSetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* eventInfo)
{
	KxVFSMirror_FileHandle* mirrorHandle = (KxVFSMirror_FileHandle*)eventInfo->DokanFileInfo->Context;
	if (mirrorHandle)
	{
		if (!SetFileInformationByHandle(mirrorHandle->FileHandle, FileBasicInfo, eventInfo->Info, (DWORD)sizeof(FILE_BASIC_INFORMATION)))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	return STATUS_INVALID_HANDLE;
}

NTSTATUS KxVFSMirror::OnFindFiles(DOKAN_FIND_FILES_EVENT* eventInfo)
{
	KxDynamicString targetPath = GetTargetPath(eventInfo->PathName);
	size_t sTargetPathLength = targetPath.length();

	if (targetPath[sTargetPathLength - 1] != L'\\')
	{
		targetPath[sTargetPathLength++] = L'\\';
	}
	targetPath[sTargetPathLength] = L'*';
	targetPath[sTargetPathLength + 1] = L'\0';

	WIN32_FIND_DATAW findData = {0};
	HANDLE findHandle = FindFirstFileW(targetPath, &findData);

	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	// Root folder does not have . and .. folder - we remove them
	bool bRootFolder = (wcscmp(eventInfo->PathName, L"\\") == 0);

	size_t count = 0;
	do
	{
		if (!bRootFolder || (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
		{
			eventInfo->FillFindData(eventInfo, &findData);
		}
		count++;
	}
	while (FindNextFileW(findHandle, &findData) != 0);

	DWORD errorCode = GetLastError();
	FindClose(findHandle);

	if (errorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
NTSTATUS KxVFSMirror::OnFindStreams(DOKAN_FIND_STREAMS_EVENT* eventInfo)
{
	KxDynamicString targetPath = GetTargetPath(eventInfo->FileName);

	WIN32_FIND_STREAM_DATA findData;
	HANDLE findHandle = FindFirstStreamW(targetPath, FindStreamInfoStandard, &findData, 0);
	if (findHandle == INVALID_HANDLE_VALUE)
	{
		return GetNtStatusByWin32LastErrorCode();
	}

	DWORD errorCode = 0;
	size_t count = 0;

	DOKAN_STREAM_FIND_RESULT findResult = DOKAN_STREAM_BUFFER_CONTINUE;
	if ((findResult = eventInfo->FillFindStreamData(eventInfo, &findData)) == DOKAN_STREAM_BUFFER_CONTINUE)
	{
		count++;
		while (FindNextStreamW(findHandle, &findData) != 0 && (findResult = eventInfo->FillFindStreamData(eventInfo, &findData)) == DOKAN_STREAM_BUFFER_CONTINUE)
		{
			count++;
		}
	}

	errorCode = GetLastError();
	FindClose(findHandle);

	if (findResult == DOKAN_STREAM_BUFFER_FULL)
	{
		// FindStreams returned 'count' entries in 'sInSourcePath' with STATUS_BUFFER_OVERFLOW
		// https://msdn.microsoft.com/en-us/library/windows/hardware/ff540364(v=vs.85).aspx
		return STATUS_BUFFER_OVERFLOW;
	}

	if (errorCode != ERROR_HANDLE_EOF)
	{
		return GetNtStatusByWin32ErrorCode(errorCode);
	}
	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
void KxVFSMirror::FreeMirrorFileHandle(KxVFSMirror_FileHandle* fileHandle)
{
	if (fileHandle)
	{
		#if KxVFS_USE_ASYNC_IO
		if (fileHandle->IoCompletion)
		{
			{
				KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
				if (m_ThreadPoolCleanupGroup)
				{
					CloseThreadpoolIo(fileHandle->IoCompletion);
				}
			}
			fileHandle->IoCompletion = NULL;
		}
		#endif

		delete fileHandle;
	}
}
void KxVFSMirror::PushMirrorFileHandle(KxVFSMirror_FileHandle* fileHandle)
{
	fileHandle->self = this;

	#if KxVFS_USE_ASYNC_IO
	if (fileHandle->IoCompletion)
	{
		{
			KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
			if (m_ThreadPoolCleanupGroup)
			{
				CloseThreadpoolIo(fileHandle->IoCompletion);
			}
		}
		fileHandle->IoCompletion = NULL;
	}
	#endif

	KxVFSCriticalSectionLocker lock(m_FileHandlePoolCS);
	{
		DokanVector_PushBack(&m_FileHandlePool, &fileHandle);
	}
}
KxVFSMirror_FileHandle* KxVFSMirror::PopMirrorFileHandle(HANDLE actualFileHandle)
{
	if (actualFileHandle == NULL || actualFileHandle == INVALID_HANDLE_VALUE || InterlockedAdd(&m_IsUnmounted, 0) != FALSE)
	{
		return NULL;
	}

	KxVFSMirror_FileHandle* mirrorHandle = NULL;
	KxVFSCriticalSectionLocker lock(m_FileHandlePoolCS);
	{
		if (DokanVector_GetCount(&m_FileHandlePool) > 0)
		{
			mirrorHandle = *(KxVFSMirror_FileHandle**)DokanVector_GetLastItem(&m_FileHandlePool);
			DokanVector_PopBack(&m_FileHandlePool);
		}
	}

	if (!mirrorHandle)
	{
		mirrorHandle = new KxVFSMirror_FileHandle(this);
	}

	mirrorHandle->FileHandle = actualFileHandle;
	mirrorHandle->IsCleanedUp = false;
	mirrorHandle->IsClosed = false;

	#if KxVFS_USE_ASYNC_IO
	{
		KxVFSCriticalSectionLocker lock(m_ThreadPoolCS);
		if (m_ThreadPoolCleanupGroup)
		{
			mirrorHandle->IoCompletion = CreateThreadpoolIo(actualFileHandle, MirrorIoCallback, mirrorHandle, &m_ThreadPoolEnvironment);
		}
	}

	if (!mirrorHandle->IoCompletion)
	{
		PushMirrorFileHandle(mirrorHandle);
		mirrorHandle = NULL;
	}
	#endif
	return mirrorHandle;
}
void KxVFSMirror::GetMirrorFileHandleState(KxVFSMirror_FileHandle* fileHandle, bool* isCleanedUp, bool* isClosed) const
{
	if (fileHandle && (isCleanedUp || isClosed))
	{
		KxVFSCriticalSectionLocker lock(fileHandle->Lock);

		if (isCleanedUp)
		{
			*isCleanedUp = fileHandle->IsCleanedUp;
		}
		if (isClosed)
		{
			*isClosed = fileHandle->IsClosed;
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
void KxVFSMirror::FreeMirrorOverlapped(KxVFSMirror_Overlapped* overlapped) const
{
	delete overlapped;
}
void KxVFSMirror::PushMirrorOverlapped(KxVFSMirror_Overlapped* overlapped)
{
	KxVFSCriticalSectionLocker lock(m_OverlappedPoolCS);
	DokanVector_PushBack(&m_OverlappedPool, &overlapped);
}
KxVFSMirror_Overlapped* KxVFSMirror::PopMirrorOverlapped()
{
	KxVFSMirror_Overlapped* overlapped = NULL;
	{
		KxVFSCriticalSectionLocker lock(m_OverlappedPoolCS);
		if (DokanVector_GetCount(&m_OverlappedPool) > 0)
		{
			overlapped = *(KxVFSMirror_Overlapped**)DokanVector_GetLastItem(&m_OverlappedPool);
			DokanVector_PopBack(&m_OverlappedPool);
		}
	}

	if (!overlapped)
	{
		overlapped = new KxVFSMirror_Overlapped();
	}
	return overlapped;
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

	KxVFSMirror_FileHandle* mirrorHandle = reinterpret_cast<KxVFSMirror_FileHandle*>(Context);
	KxVFSMirror_Overlapped* overlapped = (KxVFSMirror_Overlapped*)Overlapped;
	DOKAN_READ_FILE_EVENT* readFileEvent = NULL;
	DOKAN_WRITE_FILE_EVENT* writeFileEvent = NULL;

	switch (overlapped->IoType)
	{
		case MIRROR_IOTYPE_READ:
		{
			readFileEvent = (DOKAN_READ_FILE_EVENT*)overlapped->Context;
			readFileEvent->NumberOfBytesRead = (DWORD)NumberOfBytesTransferred;
			DokanEndDispatchRead(readFileEvent, DokanNtStatusFromWin32(IoResult));
			break;
		}
		case MIRROR_IOTYPE_WRITE:
		{
			writeFileEvent = (DOKAN_WRITE_FILE_EVENT*)overlapped->Context;
			writeFileEvent->NumberOfBytesWritten = (DWORD)NumberOfBytesTransferred;
			DokanEndDispatchWrite(writeFileEvent, DokanNtStatusFromWin32(IoResult));
			break;
		}
	};
	mirrorHandle->self->PushMirrorOverlapped(overlapped);
}
#endif
