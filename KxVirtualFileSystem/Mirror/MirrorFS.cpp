/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Utility.h"
#include "MirrorFS.h"
#include "MirrorStructs.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

namespace KxVFS
{
	DWORD MirrorFS::GetParentSecurity(const WCHAR* filePath, PSECURITY_DESCRIPTOR* parentSecurity) const
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
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			parentSecurity
		);
	}
	DWORD MirrorFS::CreateNewSecurity(EvtCreateFile& eventInfo, const WCHAR* filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const
	{
		if (!filePath || !requestedSecurity || !newSecurity)
		{
			return ERROR_INVALID_PARAMETER;
		}

		int errorCode = ERROR_SUCCESS;
		PSECURITY_DESCRIPTOR parentDescriptor = nullptr;
		if ((errorCode = GetParentSecurity(filePath, &parentDescriptor)) == ERROR_SUCCESS)
		{
			static GENERIC_MAPPING g_GenericMapping =
			{
				FILE_GENERIC_READ,
				FILE_GENERIC_WRITE,
				FILE_GENERIC_EXECUTE,
				FILE_ALL_ACCESS
			};

			HANDLE aaccessTokenHandle = DokanOpenRequestorToken(eventInfo.DokanFileInfo);
			if (aaccessTokenHandle && aaccessTokenHandle != INVALID_HANDLE_VALUE)
			{
				if (!CreatePrivateObjectSecurity(parentDescriptor,
												 eventInfo.SecurityContext.AccessState.SecurityDescriptor,
												 newSecurity,
												 eventInfo.DokanFileInfo->IsDirectory,
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

	bool MirrorFS::CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, const WCHAR* filePath) const
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
	NTSTATUS MirrorFS::CanDeleteDirectory(const WCHAR* filePath) const
	{
		KxDynamicStringW filePathCopy = filePath;

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
	NTSTATUS MirrorFS::ReadFileSynchronous(EvtReadFile& eventInfo, HANDLE fileHandle) const
	{
		LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = eventInfo.Offset;

		if (!SetFilePointerEx(fileHandle, distanceToMove, nullptr, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		if (ReadFile(fileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToRead, &eventInfo.NumberOfBytesRead, nullptr))
		{
			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
	NTSTATUS MirrorFS::WriteFileSynchronous(EvtWriteFile& eventInfo, HANDLE fileHandle, UINT64 fileSize) const
	{
		LARGE_INTEGER distanceToMove;

		if (eventInfo.DokanFileInfo->WriteToEndOfFile)
		{
			LARGE_INTEGER pos;
			pos.QuadPart = 0;

			if (!SetFilePointerEx(fileHandle, pos, nullptr, FILE_END))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
		}
		else
		{
			// Paging IO cannot write after allocate file size.
			if (eventInfo.DokanFileInfo->PagingIo)
			{
				if ((UINT64)eventInfo.Offset >= fileSize)
				{
					eventInfo.NumberOfBytesWritten = 0;
					return STATUS_SUCCESS;
				}

				// WFT is happening here?
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

			if ((UINT64)eventInfo.Offset > fileSize)
			{
				// In the mirror sample helperZeroFileData is not necessary. NTFS will zero a hole.
				// But if user's file system is different from NTFS (or other Windows
				// file systems) then  users will have to zero the hole themselves.

				// If only Dokan devs can explain more clearly what they are talking about
			}

			distanceToMove.QuadPart = eventInfo.Offset;
			if (!SetFilePointerEx(fileHandle, distanceToMove, nullptr, FILE_BEGIN))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
		}

		if (WriteFile(fileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, &eventInfo.NumberOfBytesWritten, nullptr))
		{
			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}

	void MirrorFS::ResolveLocation(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath)
	{
		targetPath = L"\\\\?\\";
		targetPath.append(m_Source);
		targetPath.append(requestedPath);
	}
}

namespace KxVFS
{
	MirrorFS::MirrorFS(Service* vfsService, KxDynamicStringRefW mountPoint, KxDynamicStringRefW source, uint32_t flags)
		:AbstractFS(vfsService, mountPoint, flags), m_Source(source)
	{
	}
	MirrorFS::~MirrorFS()
	{
	}

	bool MirrorFS::SetSource(KxDynamicStringRefW source)
	{
		if (!IsMounted())
		{
			m_Source = source;
			return true;
		}
		return false;
	}

	int MirrorFS::Mount()
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
			return AbstractFS::Mount();
		}
		return DOKAN_ERROR;
	}
}

namespace KxVFS
{
	NTSTATUS MirrorFS::OnMount(EvtMounted& eventInfo)
	{
		return STATUS_NOT_SUPPORTED;
	}
	NTSTATUS MirrorFS::OnUnMount(EvtUnMounted& eventInfo)
	{
		CleanupMirrorFileHandles();
		#if KxVFS_USE_ASYNC_IO
		CleanupAsyncIO();
		#endif

		return STATUS_NOT_SUPPORTED;
	}

	NTSTATUS MirrorFS::OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo)
	{
		if (!GetDiskFreeSpaceExW(m_Source.data(), (ULARGE_INTEGER*)&eventInfo.FreeBytesAvailable, (ULARGE_INTEGER*)&eventInfo.TotalNumberOfBytes, (ULARGE_INTEGER*)&eventInfo.TotalNumberOfFreeBytes))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	NTSTATUS MirrorFS::OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo)
	{
		eventInfo.VolumeInfo->VolumeSerialNumber = GetVolumeSerialNumber();
		eventInfo.VolumeInfo->VolumeLabelLength = WriteString(GetVolumeName().data(), eventInfo.VolumeInfo->VolumeLabel, eventInfo.MaxLabelLengthInChars);
		eventInfo.VolumeInfo->SupportsObjects = FALSE;

		return STATUS_SUCCESS;
	}
	NTSTATUS MirrorFS::OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo)
	{
		size_t maxFileSystemNameLengthInBytes = eventInfo.MaxFileSystemNameLengthInChars * sizeof(WCHAR);

		eventInfo.Attributes->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH|FILE_CASE_PRESERVED_NAMES|FILE_SUPPORTS_REMOTE_STORAGE|FILE_UNICODE_ON_DISK|FILE_PERSISTENT_ACLS|FILE_NAMED_STREAMS;
		eventInfo.Attributes->MaximumComponentNameLength = 256; // Not 255, this means Dokan doesn't support long file names?

		WCHAR volumeRoot[4];
		volumeRoot[0] = m_Source[0];
		volumeRoot[1] = L':';
		volumeRoot[2] = L'\\';
		volumeRoot[3] = L'\0';

		DWORD fileSystemFlags = 0;
		DWORD maximumComponentLength = 0;

		KxDynamicStringRefW fileSystemName;
		WCHAR fileSystemNameBuffer[255] = {0};
		if (GetVolumeInformationW(volumeRoot, nullptr, 0, nullptr, &maximumComponentLength, &fileSystemFlags, fileSystemNameBuffer, std::size(fileSystemNameBuffer)))
		{
			eventInfo.Attributes->MaximumComponentNameLength = maximumComponentLength;
			eventInfo.Attributes->FileSystemAttributes &= fileSystemFlags;

			fileSystemName = fileSystemNameBuffer;
		}
		eventInfo.Attributes->FileSystemNameLength = WriteString(fileSystemName.data(), eventInfo.Attributes->FileSystemName, eventInfo.MaxFileSystemNameLengthInChars);

		return STATUS_SUCCESS;
	}

	NTSTATUS MirrorFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		DWORD errorCode = 0;
		NTSTATUS statusCode = STATUS_SUCCESS;

		KxDynamicStringW targetPath;
		ResolveLocation(eventInfo.FileName, targetPath);

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

		SECURITY_DESCRIPTOR* fileSecurity = nullptr;
		KxCallAtScopeExit destroyFileSecurityAtExit([&fileSecurity]()
		{
			if (fileSecurity)
			{
				::DestroyPrivateObjectSecurity(reinterpret_cast<PSECURITY_DESCRIPTOR*>(&fileSecurity));
				fileSecurity = nullptr;
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
				if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
				{
					return STATUS_NOT_A_DIRECTORY;
				}

				// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
				FileHandle fileHandle = CreateFileW
				(
					targetPath,
					genericDesiredAccess,
					eventInfo.ShareAccess,
					&securityAttributes,
					OPEN_EXISTING,
					fileAttributesAndFlags|FILE_FLAG_BACKUP_SEMANTICS,
					nullptr
				);

				if (fileHandle.IsOK())
				{
					Mirror::FileContext* mirrorContext = PopMirrorFileHandle(fileHandle);
					if (!mirrorContext)
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
					eventInfo.DokanFileInfo->Context = (ULONG64)mirrorContext;

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

				KxVFSDebugPrint(L"Trying to create/open file: %s", targetPath.data());
				FileHandle fileHandle = CreateFileW(targetPath,
														 genericDesiredAccess, // GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
														 eventInfo.ShareAccess,
														 &securityAttributes,
														 creationDisposition,
														 fileAttributesAndFlags, // |FILE_FLAG_NO_BUFFERING,
														 nullptr
				);
				errorCode = GetLastError();

				if (!fileHandle.IsOK())
				{
					KxVFSDebugPrint(L"Failed to create/open file: %s", targetPath.data());
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
				}
				else
				{
					// Need to update FileAttributes with previous when Overwrite file
					if (fileAttributes != INVALID_FILE_ATTRIBUTES && creationDisposition == TRUNCATE_EXISTING)
					{
						SetFileAttributesW(targetPath, fileAttributesAndFlags|fileAttributes);
					}

					Mirror::FileContext* mirrorContext = PopMirrorFileHandle(fileHandle);
					if (!mirrorContext)
					{
						SetLastError(ERROR_INTERNAL_ERROR);
						statusCode = STATUS_INTERNAL_ERROR;
					}
					else
					{
						// Save the file handle in m_Context
						fileHandle.Release();
						eventInfo.DokanFileInfo->Context = (ULONG64)mirrorContext;

						if (creationDisposition == OPEN_ALWAYS || creationDisposition == CREATE_ALWAYS)
						{
							if (errorCode == ERROR_ALREADY_EXISTS)
							{
								// Open succeed but we need to inform the driver,
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
	NTSTATUS MirrorFS::OnCloseFile(EvtCloseFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			{
				CriticalSectionLocker lock(mirrorContext->m_Lock);

				mirrorContext->m_IsClosed = true;
				if (mirrorContext->m_FileHandle && mirrorContext->m_FileHandle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(mirrorContext->m_FileHandle);
					mirrorContext->m_FileHandle = nullptr;

					KxDynamicStringW targetPath;
					ResolveLocation(eventInfo.FileName, targetPath);
					CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath);
				}
			}

			eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(nullptr);
			PushMirrorFileHandle(mirrorContext);
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnCleanUp(EvtCleanUp& eventInfo)
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

		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			{
				CriticalSectionLocker lock(mirrorContext->m_Lock);

				CloseHandle(mirrorContext->m_FileHandle);

				mirrorContext->m_FileHandle = nullptr;
				mirrorContext->m_IsCleanedUp = true;

				KxDynamicStringW targetPath;
				ResolveLocation(eventInfo.FileName, targetPath);
				CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath);
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			//KxDynamicStringW targetPathOld;
			//ResolveLocation(eventInfo.FileName, targetPathOld);

			KxDynamicStringW targetPathNew;
			ResolveLocation(eventInfo.NewFileName, targetPathNew);

			// The FILE_RENAME_INFO struct has space for one WCHAR for the name at
			// the end, so that accounts for the null terminator
			DWORD bufferSize = (DWORD)(sizeof(FILE_RENAME_INFO) + targetPathNew.length() * sizeof(targetPathNew[0]));
			PFILE_RENAME_INFO renameInfo = (PFILE_RENAME_INFO)malloc(bufferSize);
			KxCallAtScopeExit atExit([renameInfo]()
			{
				free(renameInfo);
			});

			if (renameInfo == nullptr)
			{
				return STATUS_BUFFER_OVERFLOW;
			}
			ZeroMemory(renameInfo, bufferSize);

			renameInfo->ReplaceIfExists = eventInfo.ReplaceIfExists ? TRUE : FALSE; // Some warning about converting BOOL to BOOLEAN
			renameInfo->RootDirectory = nullptr; // Hope it is never needed, shouldn't be

			renameInfo->FileNameLength = (DWORD)targetPathNew.length() * sizeof(targetPathNew[0]); // They want length in bytes
			wcscpy_s(renameInfo->FileName, targetPathNew.length() + 1, targetPathNew);

			BOOL result = SetFileInformationByHandle(mirrorContext->m_FileHandle, FileRenameInfo, renameInfo, bufferSize);
			

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
	NTSTATUS MirrorFS::OnCanDeleteFile(EvtCanDeleteFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			BY_HANDLE_FILE_INFORMATION fileInfo = {0};
			if (!GetFileInformationByHandle(mirrorContext->m_FileHandle, &fileInfo))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY)
			{
				return STATUS_CANNOT_DELETE;
			}

			if (eventInfo.DokanFileInfo->IsDirectory)
			{
				KxDynamicStringW targetPath;
				ResolveLocation(eventInfo.FileName, targetPath);
				return CanDeleteDirectory(targetPath);
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}

	NTSTATUS MirrorFS::OnLockFile(EvtLockFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			LARGE_INTEGER length;
			length.QuadPart = eventInfo.Length;

			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.ByteOffset;

			if (!LockFile(mirrorContext->m_FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnUnlockFile(EvtUnlockFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			LARGE_INTEGER length;
			length.QuadPart = eventInfo.Length;

			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.ByteOffset;

			if (!UnlockFile(mirrorContext->m_FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnGetFileSecurity(EvtGetFileSecurity& eventInfo)
	{
		// This doesn't work properly anyway
		return STATUS_NOT_IMPLEMENTED;

		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			PSECURITY_DESCRIPTOR tempSecurityDescriptor = nullptr;

			// GetSecurityInfo() is not thread safe so we use a critical section here to synchronize
			DWORD errorCode = ERROR_SUCCESS;
			{
				CriticalSectionLocker lock(mirrorContext->m_Lock);

				if (mirrorContext->m_IsClosed)
				{
					errorCode = ERROR_INVALID_HANDLE;
				}
				else if (mirrorContext->m_IsCleanedUp)
				{
					KxDynamicStringW targetPath;
					ResolveLocation(eventInfo.FileName, targetPath);
					errorCode = GetNamedSecurityInfoW(targetPath, SE_FILE_OBJECT, eventInfo.SecurityInformation, nullptr, nullptr, nullptr, nullptr, &tempSecurityDescriptor);
				}
				else
				{
					errorCode = GetSecurityInfo(mirrorContext->m_FileHandle, SE_FILE_OBJECT, eventInfo.SecurityInformation, nullptr, nullptr, nullptr, nullptr, &tempSecurityDescriptor);
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
				if (!MakeSelfRelativeSD(tempSecurityDescriptor, eventInfo.SecurityDescriptor, &eventInfo.LengthNeeded))
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
				eventInfo.LengthNeeded = GetSecurityDescriptorLength(tempSecurityDescriptor);
				if (eventInfo.LengthNeeded > eventInfo.SecurityDescriptorSize)
				{
					LocalFree(tempSecurityDescriptor);
					return STATUS_BUFFER_OVERFLOW;
				}
				memcpy_s(eventInfo.SecurityDescriptor, eventInfo.SecurityDescriptorSize, tempSecurityDescriptor, eventInfo.LengthNeeded);
			}
			LocalFree(tempSecurityDescriptor);

			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnSetFileSecurity(EvtSetFileSecurity& eventInfo)
	{
		// And this can be problematic too
		return STATUS_NOT_IMPLEMENTED;

		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			// SecurityDescriptor must be 4-byte aligned
			_ASSERT(((size_t)eventInfo.SecurityDescriptor & 3) == 0);

			BOOL setSecurity = FALSE;

			{
				CriticalSectionLocker lock(mirrorContext->m_Lock);

				if (mirrorContext->m_IsClosed)
				{
					setSecurity = FALSE;
					SetLastError(ERROR_INVALID_HANDLE);
				}
				else if (mirrorContext->m_IsCleanedUp)
				{
					// I wonder why it was commented out
					#if 0
					PSID owner = nullptr;
					PSID group = nullptr;
					PACL dacl = nullptr;
					PACL sacl = nullptr;
					BOOL ownerDefault = FALSE;

					if (eventInfo.SecurityInformation & (OWNER_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
					{
						GetSecurityDescriptorOwner(eventInfo.SecurityDescriptor, &owner, &ownerDefault);
					}

					if (eventInfo.SecurityInformation & (GROUP_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
					{
						GetSecurityDescriptorGroup(eventInfo.SecurityDescriptor, &group, &ownerDefault);
					}

					if (eventInfo.SecurityInformation & (DACL_SECURITY_INFORMATION | BACKUP_SECURITY_INFORMATION))
					{
						BOOL hasDacl = FALSE;
						GetSecurityDescriptorDacl(eventInfo.SecurityDescriptor, &hasDacl, &dacl, &ownerDefault);
					}

					if (eventInfo.SecurityInformation &
						(GROUP_SECURITY_INFORMATION
						 | BACKUP_SECURITY_INFORMATION
						 | LABEL_SECURITY_INFORMATION
						 | SACL_SECURITY_INFORMATION
						 | SCOPE_SECURITY_INFORMATION
						 | ATTRIBUTE_SECURITY_INFORMATION))
					{
						BOOL hasSacl = FALSE;
						GetSecurityDescriptorSacl(eventInfo.SecurityDescriptor, &hasSacl, &sacl, &ownerDefault);
					}
					setSecurity = SetNamedSecurityInfoW(filePath, SE_FILE_OBJECT, eventInfo.SecurityInformation, owner, group, dacl, sacl) == ERROR_SUCCESS;
					#endif

					KxDynamicStringW targetPath;
					ResolveLocation(eventInfo.FileName, targetPath);
					setSecurity = SetFileSecurityW(targetPath, eventInfo.SecurityInformation, eventInfo.SecurityDescriptor);
				}
				else
				{
					// For some reason this appears to be only variant of SetSecurity that works without returning an ERROR_ACCESS_DENIED
					setSecurity = SetUserObjectSecurity(mirrorContext->m_FileHandle, &eventInfo.SecurityInformation, eventInfo.SecurityDescriptor);
				}
			}

			if (!setSecurity)
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}

	NTSTATUS MirrorFS::OnReadFile(EvtReadFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (!mirrorContext)
		{
			return STATUS_FILE_CLOSED;
		}

		bool isCleanedUp = false;
		bool isClosed = false;
		GetMirrorFileHandleState(mirrorContext, &isCleanedUp, &isClosed);

		if (isClosed)
		{
			return STATUS_FILE_CLOSED;
		}

		if (isCleanedUp)
		{
			KxDynamicStringW targetPath;
			ResolveLocation(eventInfo.FileName, targetPath);
			FileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (tempFileHandle.IsOK())
			{
				return ReadFileSynchronous(eventInfo, tempFileHandle);
			}
			return GetNtStatusByWin32LastErrorCode();
		}

		#if KxVFS_USE_ASYNC_IO
		Mirror::OverlappedContext* overlapped = PopMirrorOverlapped();
		if (!overlapped)
		{
			return STATUS_MEMORY_NOT_ALLOCATED;
		}

		Utility::Int64ToOverlappedOffset(eventInfo.Offset, overlapped->m_InternalOverlapped);
		overlapped->m_FileHandle = mirrorContext;
		overlapped->m_Context = &eventInfo;
		overlapped->m_IOType = Mirror::IOOperationType::Read;

		StartThreadpoolIo(mirrorContext->m_IOCompletion);
		if (!ReadFile(mirrorContext->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToRead, &eventInfo.NumberOfBytesRead, (LPOVERLAPPED)overlapped))
		{
			DWORD errorCode = GetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(mirrorContext->m_IOCompletion);
				return GetNtStatusByWin32ErrorCode(errorCode);
			}
		}

		return STATUS_PENDING;
		#else
		return ReadFileSynchronous(eventInfo, mirrorContext->m_FileHandle);
		#endif
	}
	NTSTATUS MirrorFS::OnWriteFile(EvtWriteFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (!mirrorContext)
		{
			return STATUS_FILE_CLOSED;
		}

		bool isCleanedUp = false;
		bool isClosed = false;
		GetMirrorFileHandleState(mirrorContext, &isCleanedUp, &isClosed);
		if (isClosed)
		{
			return STATUS_FILE_CLOSED;
		}

		DWORD fileSizeHigh = 0;
		DWORD fileSizeLow = 0;
		UINT64 fileSize = 0;
		if (isCleanedUp)
		{
			KxDynamicStringW targetPath;
			ResolveLocation(eventInfo.FileName, targetPath);
			FileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
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

			// Need to check if its really needs to be 'mirrorContext->m_FileHandle' and not 'tempFileHandle'
			return WriteFileSynchronous(eventInfo, mirrorContext->m_FileHandle, fileSize);
		}

		fileSizeLow = ::GetFileSize(mirrorContext->m_FileHandle, &fileSizeHigh);
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

		Mirror::OverlappedContext* overlapped = PopMirrorOverlapped();
		if (!overlapped)
		{
			return STATUS_MEMORY_NOT_ALLOCATED;
		}


		Utility::Int64ToOverlappedOffset(eventInfo.Offset, overlapped->m_InternalOverlapped);
		overlapped->m_FileHandle = mirrorContext;
		overlapped->m_Context = &eventInfo;
		overlapped->m_IOType = Mirror::IOOperationType::Write;

		StartThreadpoolIo(mirrorContext->m_IOCompletion);
		if (!WriteFile(mirrorContext->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, &eventInfo.NumberOfBytesWritten, (LPOVERLAPPED)overlapped))
		{
			DWORD errorCode = GetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				CancelThreadpoolIo(mirrorContext->m_IOCompletion);
				return GetNtStatusByWin32ErrorCode(errorCode);
			}
		}
		return STATUS_PENDING;
		#else
		return WriteFileSynchronous(eventInfo, mirrorContext->m_FileHandle, fileSize);
		#endif
	}
	NTSTATUS MirrorFS::OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (!mirrorContext)
		{
			return STATUS_FILE_CLOSED;
		}

		if (FlushFileBuffers(mirrorContext->m_FileHandle))
		{
			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
	NTSTATUS MirrorFS::OnSetEndOfFile(EvtSetEndOfFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.Length;

			if (!SetFilePointerEx(mirrorContext->m_FileHandle, offset, nullptr, FILE_BEGIN))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if (!SetEndOfFile(mirrorContext->m_FileHandle))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnSetAllocationSize(EvtSetAllocationSize& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			LARGE_INTEGER fileSize;
			if (GetFileSizeEx(mirrorContext->m_FileHandle, &fileSize))
			{
				if (eventInfo.Length < fileSize.QuadPart)
				{
					fileSize.QuadPart = eventInfo.Length;

					if (!SetFilePointerEx(mirrorContext->m_FileHandle, fileSize, nullptr, FILE_BEGIN))
					{
						return GetNtStatusByWin32LastErrorCode();
					}
					if (!SetEndOfFile(mirrorContext->m_FileHandle))
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
	NTSTATUS MirrorFS::OnGetFileInfo(EvtGetFileInfo& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			if (!GetFileInformationByHandle(mirrorContext->m_FileHandle, &eventInfo.FileHandleInfo))
			{
				KxDynamicStringW targetPath;
				ResolveLocation(eventInfo.FileName, targetPath);
				KxVFSDebugPrint(L"Couldn't get file info by handle, trying by file name: %s", targetPath.data());

				// FileName is a root directory, in this case, 'FindFirstFile' can't get directory information.
				if (wcslen(eventInfo.FileName) == 1)
				{
					eventInfo.FileHandleInfo.dwFileAttributes = ::GetFileAttributesW(targetPath);
				}
				else
				{
					WIN32_FIND_DATAW findData = {0};
					HANDLE fileHandle = FindFirstFileW(targetPath, &findData);
					if (fileHandle == INVALID_HANDLE_VALUE)
					{
						return GetNtStatusByWin32LastErrorCode();
					}

					eventInfo.FileHandleInfo.dwFileAttributes = findData.dwFileAttributes;
					eventInfo.FileHandleInfo.ftCreationTime = findData.ftCreationTime;
					eventInfo.FileHandleInfo.ftLastAccessTime = findData.ftLastAccessTime;
					eventInfo.FileHandleInfo.ftLastWriteTime = findData.ftLastWriteTime;
					eventInfo.FileHandleInfo.nFileSizeHigh = findData.nFileSizeHigh;
					eventInfo.FileHandleInfo.nFileSizeLow = findData.nFileSizeLow;
					FindClose(fileHandle);
				}
			}

			KxVFSDebugPrint(L"Successfully retrieved file info by handle: %s", eventInfo.FileName);
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			if (!SetFileInformationByHandle(mirrorContext->m_FileHandle, FileBasicInfo, eventInfo.Info, (DWORD)sizeof(Dokany2::FILE_BASIC_INFORMATION)))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}

	NTSTATUS MirrorFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		KxDynamicStringW targetPath;
		ResolveLocation(eventInfo.PathName, targetPath);
		size_t targetPathLength = targetPath.length();

		if (targetPath[targetPathLength - 1] != L'\\')
		{
			targetPath[targetPathLength++] = L'\\';
		}
		targetPath[targetPathLength] = L'*';
		targetPath[targetPathLength + 1] = L'\0';

		WIN32_FIND_DATAW findData = {0};
		HANDLE findHandle = FindFirstFileW(targetPath, &findData);

		if (findHandle == INVALID_HANDLE_VALUE)
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		// Root folder does not have . and .. folder - we remove them
		const bool isRootFolder = (wcscmp(eventInfo.PathName, L"\\") == 0);

		size_t count = 0;
		do
		{
			if (!isRootFolder || (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
			{
				eventInfo.FillFindData(&eventInfo, &findData);
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
	NTSTATUS MirrorFS::OnFindStreams(EvtFindStreams& eventInfo)
	{
		KxDynamicStringW targetPath;
		ResolveLocation(eventInfo.FileName, targetPath);

		WIN32_FIND_STREAM_DATA findData;
		HANDLE findHandle = FindFirstStreamW(targetPath, FindStreamInfoStandard, &findData, 0);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		DWORD errorCode = 0;
		size_t count = 0;

		Dokany2::DOKAN_STREAM_FIND_RESULT findResult = Dokany2::DOKAN_STREAM_BUFFER_CONTINUE;
		if ((findResult = eventInfo.FillFindStreamData(&eventInfo, &findData)) == Dokany2::DOKAN_STREAM_BUFFER_CONTINUE)
		{
			count++;
			while (FindNextStreamW(findHandle, &findData) != 0 && (findResult = eventInfo.FillFindStreamData(&eventInfo, &findData)) == Dokany2::DOKAN_STREAM_BUFFER_CONTINUE)
			{
				count++;
			}
		}

		errorCode = GetLastError();
		FindClose(findHandle);

		if (findResult == Dokany2::DOKAN_STREAM_BUFFER_FULL)
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
}

namespace KxVFS
{
	void MirrorFS::FreeMirrorFileHandle(Mirror::FileContext* fileHandle)
	{
		if (fileHandle)
		{
			#if KxVFS_USE_ASYNC_IO
			if (fileHandle->m_IOCompletion)
			{
				{
					CriticalSectionLocker lock(m_ThreadPoolCS);
					if (m_ThreadPoolCleanupGroup)
					{
						CloseThreadpoolIo(fileHandle->m_IOCompletion);
					}
				}
				fileHandle->m_IOCompletion = nullptr;
			}
			#endif

			delete fileHandle;
		}
	}
	void MirrorFS::PushMirrorFileHandle(Mirror::FileContext* fileHandle)
	{
		fileHandle->m_VFSInstance = this;

		#if KxVFS_USE_ASYNC_IO
		if (fileHandle->m_IOCompletion)
		{
			{
				CriticalSectionLocker lock(m_ThreadPoolCS);
				if (m_ThreadPoolCleanupGroup)
				{
					CloseThreadpoolIo(fileHandle->m_IOCompletion);
				}
			}
			fileHandle->m_IOCompletion = nullptr;
		}
		#endif

		CriticalSectionLocker lock(m_FileHandlePoolCS);
		{
			DokanVector_PushBack(&m_FileHandlePool, &fileHandle);
		}
	}
	Mirror::FileContext* MirrorFS::PopMirrorFileHandle(HANDLE actualFileHandle)
	{
		if (actualFileHandle == nullptr || actualFileHandle == INVALID_HANDLE_VALUE || InterlockedAdd(&m_IsUnmounted, 0) != FALSE)
		{
			return nullptr;
		}

		Mirror::FileContext* mirrorContext = nullptr;
		CriticalSectionLocker lock(m_FileHandlePoolCS);
		{
			if (DokanVector_GetCount(&m_FileHandlePool) > 0)
			{
				mirrorContext = *(Mirror::FileContext**)DokanVector_GetLastItem(&m_FileHandlePool);
				DokanVector_PopBack(&m_FileHandlePool);
			}
		}

		if (!mirrorContext)
		{
			mirrorContext = new Mirror::FileContext(this);
		}

		mirrorContext->m_FileHandle = actualFileHandle;
		mirrorContext->m_IsCleanedUp = false;
		mirrorContext->m_IsClosed = false;

		#if KxVFS_USE_ASYNC_IO
		{
			CriticalSectionLocker lock(m_ThreadPoolCS);
			if (m_ThreadPoolCleanupGroup)
			{
				mirrorContext->m_IOCompletion = CreateThreadpoolIo(actualFileHandle, MirrorIoCallback, mirrorContext, &m_ThreadPoolEnvironment);
			}
		}

		if (!mirrorContext->m_IOCompletion)
		{
			PushMirrorFileHandle(mirrorContext);
			mirrorContext = nullptr;
		}
		#endif
		return mirrorContext;
	}
	void MirrorFS::GetMirrorFileHandleState(Mirror::FileContext* fileHandle, bool* isCleanedUp, bool* isClosed) const
	{
		if (fileHandle && (isCleanedUp || isClosed))
		{
			CriticalSectionLocker lock(fileHandle->m_Lock);

			if (isCleanedUp)
			{
				*isCleanedUp = fileHandle->m_IsCleanedUp;
			}
			if (isClosed)
			{
				*isClosed = fileHandle->m_IsClosed;
			}
		}
	}

	BOOL MirrorFS::InitializeMirrorFileHandles()
	{
		return DokanVector_StackAlloc(&m_FileHandlePool, sizeof(Mirror::FileContext*));
	}
	void MirrorFS::CleanupMirrorFileHandles()
	{
		CriticalSectionLocker lock(m_FileHandlePoolCS);

		for (size_t i = 0; i < DokanVector_GetCount(&m_FileHandlePool); ++i)
		{
			FreeMirrorFileHandle(*(Mirror::FileContext**)DokanVector_GetItem(&m_FileHandlePool, i));
		}
		DokanVector_Free(&m_FileHandlePool);
	}
}

namespace KxVFS
{
	#if KxVFS_USE_ASYNC_IO
	void MirrorFS::FreeMirrorOverlapped(Mirror::OverlappedContext* overlapped) const
	{
		delete overlapped;
	}
	void MirrorFS::PushMirrorOverlapped(Mirror::OverlappedContext* overlapped)
	{
		CriticalSectionLocker lock(m_OverlappedPoolCS);
		DokanVector_PushBack(&m_OverlappedPool, &overlapped);
	}
	Mirror::OverlappedContext* MirrorFS::PopMirrorOverlapped()
	{
		Mirror::OverlappedContext* overlapped = nullptr;
		{
			CriticalSectionLocker lock(m_OverlappedPoolCS);
			if (DokanVector_GetCount(&m_OverlappedPool) > 0)
			{
				overlapped = *(Mirror::OverlappedContext**)DokanVector_GetLastItem(&m_OverlappedPool);
				DokanVector_PopBack(&m_OverlappedPool);
			}
		}

		if (!overlapped)
		{
			overlapped = new Mirror::OverlappedContext();
		}
		return overlapped;
	}

	BOOL MirrorFS::InitializeAsyncIO()
	{
		m_ThreadPool = Dokany2::DokanGetThreadPool();
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
		SetThreadpoolCallbackCleanupGroup(&m_ThreadPoolEnvironment, m_ThreadPoolCleanupGroup, nullptr);

		DokanVector_StackAlloc(&m_OverlappedPool, sizeof(Mirror::OverlappedContext*));
		return TRUE;
	}
	void MirrorFS::CleanupPendingAsyncIO()
	{
		CriticalSectionLocker lock(m_ThreadPoolCS);

		if (m_ThreadPoolCleanupGroup)
		{
			CloseThreadpoolCleanupGroupMembers(m_ThreadPoolCleanupGroup, FALSE, nullptr);
			CloseThreadpoolCleanupGroup(m_ThreadPoolCleanupGroup);
			m_ThreadPoolCleanupGroup = nullptr;

			DestroyThreadpoolEnvironment(&m_ThreadPoolEnvironment);
		}
	}
	void MirrorFS::CleanupAsyncIO()
	{
		CleanupPendingAsyncIO();

		{
			CriticalSectionLocker lock(m_OverlappedPoolCS);
			for (size_t i = 0; i < DokanVector_GetCount(&m_OverlappedPool); ++i)
			{
				FreeMirrorOverlapped(*(Mirror::OverlappedContext**)DokanVector_GetItem(&m_OverlappedPool, i));
			}
			DokanVector_Free(&m_OverlappedPool);
		}
	}
	void CALLBACK MirrorFS::MirrorIoCallback
	(
		PTP_CALLBACK_INSTANCE instance,
		PVOID context,
		PVOID overlappedBuffer,
		ULONG resultIO,
		ULONG_PTR numberOfBytesTransferred,
		PTP_IO portIO
	)
	{
		UNREFERENCED_PARAMETER(instance);
		UNREFERENCED_PARAMETER(portIO);

		Mirror::FileContext* mirrorContext = reinterpret_cast<Mirror::FileContext*>(context);
		Mirror::OverlappedContext* overlapped = (Mirror::OverlappedContext*)overlappedBuffer;
		EvtReadFile* readFileEvent = nullptr;
		EvtWriteFile* writeFileEvent = nullptr;

		switch (overlapped->m_IOType)
		{
			case Mirror::IOOperationType::Read:
			{
				readFileEvent = (EvtReadFile*)overlapped->m_Context;
				readFileEvent->NumberOfBytesRead = (DWORD)numberOfBytesTransferred;
				DokanEndDispatchRead(readFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
			case Mirror::IOOperationType::Write:
			{
				writeFileEvent = (EvtWriteFile*)overlapped->m_Context;
				writeFileEvent->NumberOfBytesWritten = (DWORD)numberOfBytesTransferred;
				DokanEndDispatchWrite(writeFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
		};
		mirrorContext->m_VFSInstance->PushMirrorOverlapped(overlapped);
	}
	#endif
}
