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

// Security section
namespace KxVFS
{
	DWORD MirrorFS::GetParentSecurity(KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR* parentSecurity) const
	{
		constexpr size_t maxPathLength = 32768;

		int lastPathSeparator = -1;
		for (int i = 0; i < maxPathLength && filePath[i]; ++i)
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

		wchar_t parentPath[maxPathLength] = {0};
		memcpy_s(parentPath, maxPathLength * sizeof(wchar_t), filePath.data(), lastPathSeparator * sizeof(wchar_t));
		parentPath[lastPathSeparator] = 0;

		// Must LocalFree() parentSecurity
		return GetNamedSecurityInfoW(parentPath,
									 SE_FILE_OBJECT,
									 BACKUP_SECURITY_INFORMATION, // give us everything
									 nullptr,
									 nullptr,
									 nullptr,
									 nullptr,
									 parentSecurity
		);
	}
	DWORD MirrorFS::CreateNewSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const
	{
		if (filePath.empty() || !requestedSecurity || !newSecurity)
		{
			return ERROR_INVALID_PARAMETER;
		}

		PSECURITY_DESCRIPTOR parentDescriptor = nullptr;
		DWORD errorCode = GetParentSecurity(filePath, &parentDescriptor);
		if (errorCode == ERROR_SUCCESS)
		{
			static GENERIC_MAPPING g_GenericMapping =
			{
				FILE_GENERIC_READ,
				FILE_GENERIC_WRITE,
				FILE_GENERIC_EXECUTE,
				FILE_ALL_ACCESS
			};

			HANDLE accessTokenHandle = Dokany2::DokanOpenRequestorToken(eventInfo.DokanFileInfo);
			if (accessTokenHandle != nullptr && accessTokenHandle != INVALID_HANDLE_VALUE)
			{
				bool success = ::CreatePrivateObjectSecurity(parentDescriptor,
															 eventInfo.SecurityContext.AccessState.SecurityDescriptor,
															 newSecurity,
															 eventInfo.DokanFileInfo->IsDirectory,
															 accessTokenHandle,
															 &g_GenericMapping
				);
				if (!success)
				{
					errorCode = ::GetLastError();
				}
				::CloseHandle(accessTokenHandle);
			}
			else
			{
				errorCode = ::GetLastError();
			}
			::LocalFree(parentDescriptor);
		}
		return errorCode;
	}
	Utility::SecurityObject MirrorFS::CreateSecurityIfNeeded(EvtCreateFile& eventInfo, SECURITY_ATTRIBUTES& securityAttributes, KxDynamicStringRefW targetPath, CreationDisposition creationDisposition)
	{
		securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		securityAttributes.lpSecurityDescriptor = eventInfo.SecurityContext.AccessState.SecurityDescriptor;
		securityAttributes.bInheritHandle = FALSE;

		// We only need security information if there's a possibility a new file could be created
		if (wcscmp(eventInfo.FileName, L"\\") != 0 && wcscmp(eventInfo.FileName, L"/") != 0	&& creationDisposition != CreationDisposition::OpenExisting && creationDisposition != CreationDisposition::TruncateExisting)
		{
			PSECURITY_DESCRIPTOR newFileSecurity = nullptr;
			if (CreateNewSecurity(eventInfo, targetPath, eventInfo.SecurityContext.AccessState.SecurityDescriptor, &newFileSecurity) == ERROR_SUCCESS)
			{
				securityAttributes.lpSecurityDescriptor = newFileSecurity;
				return newFileSecurity;
			}
		}
		return nullptr;
	}
	
	void MirrorFS::OpenWithSecurityAccess(AccessRights& desiredAccess, bool isWriteRequest) const
	{
		desiredAccess |= AccessRights::ReadControl;
		if (isWriteRequest)
		{
			desiredAccess |= AccessRights::WriteDAC;
		}
		if (GetService().HasSeSecurityNamePrivilege())
		{
			desiredAccess |= AccessRights::SystemSecurity;
		}
	}
}

// ImpersonateCallerUser section
namespace KxVFS
{
	TokenHandle MirrorFS::ImpersonateCallerUser(EvtCreateFile& eventInfo) const
	{
		TokenHandle userTokenHandle = Dokany2::DokanOpenRequestorToken(eventInfo.DokanFileInfo);
		if (userTokenHandle)
		{
			KxVFSDebugPrint(L"DokanOpenRequestorToken: success\n");
		}
		else
		{
			// Should we return some error?
			KxVFSDebugPrint(L"DokanOpenRequestorToken: failed\n");
		}
		return userTokenHandle;
	}
	bool MirrorFS::ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const
	{
		if (userTokenHandle)
		{
			if (::ImpersonateLoggedOnUser(userTokenHandle))
			{
				KxVFSDebugPrint(L"ImpersonateLoggedOnUser: success\n");
				return true;
			}
			else
			{
				KxVFSDebugPrint(L"ImpersonateLoggedOnUser: failed\n");
				return false;
			}
		}

		KxVFSDebugPrint(L"ImpersonateLoggedOnUser: invalid token\n");
		return false;
	}
	void MirrorFS::CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NTSTATUS status) const
	{
		if (userTokenHandle)
		{
			// Clean Up operation for impersonate
			const DWORD lastError = GetLastError();

			// Keep the handle open for CreateFile
			if (status != STATUS_SUCCESS)
			{
				userTokenHandle.Close();
			}
			::RevertToSelf();

			SetLastError(lastError);
		}
	}
}

// Sync Read/Write section
namespace KxVFS
{
	NTSTATUS MirrorFS::ReadFileSync(EvtReadFile& eventInfo, HANDLE fileHandle) const
	{
		LARGE_INTEGER distanceToMove;
		distanceToMove.QuadPart = eventInfo.Offset;

		if (!SetFilePointerEx(fileHandle, distanceToMove, nullptr, FILE_BEGIN))
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		if (::ReadFile(fileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToRead, &eventInfo.NumberOfBytesRead, nullptr))
		{
			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
	NTSTATUS MirrorFS::WriteFileSync(EvtWriteFile& eventInfo, HANDLE fileHandle, uint64_t fileSize) const
	{
		LARGE_INTEGER distanceToMove;

		if (eventInfo.DokanFileInfo->WriteToEndOfFile)
		{
			LARGE_INTEGER pos;
			pos.QuadPart = 0;

			if (!::SetFilePointerEx(fileHandle, pos, nullptr, FILE_END))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
		}
		else
		{
			// Paging IO cannot write after allocated file size
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
			if (!::SetFilePointerEx(fileHandle, distanceToMove, nullptr, FILE_BEGIN))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
		}

		if (::WriteFile(fileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, &eventInfo.NumberOfBytesWritten, nullptr))
		{
			return STATUS_SUCCESS;
		}
		return GetNtStatusByWin32LastErrorCode();
	}
}

namespace KxVFS
{
	bool MirrorFS::CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, KxDynamicStringRefW filePath) const
	{
		if (fileInfo->DeleteOnClose)
		{
			KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), filePath.data());

			// Should already be deleted by CloseHandle if open with 'FILE_FLAG_DELETE_ON_CLOSE'.
			if (fileInfo->IsDirectory)
			{
				::RemoveDirectoryW(filePath.data());
			}
			else
			{
				::DeleteFileW(filePath.data());
			}
			return true;
		}
		return false;
	}
	NTSTATUS MirrorFS::CanDeleteDirectory(KxDynamicStringRefW filePath) const
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
}

// IRequestDispatcher
namespace KxVFS
{
	void MirrorFS::DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath)
	{
		targetPath = L"\\\\?\\";
		targetPath.append(m_Source);
		targetPath.append(requestedPath);
	}
}

namespace KxVFS
{
	MirrorFS::MirrorFS(Service& service, KxDynamicStringRefW mountPoint, KxDynamicStringRefW source, uint32_t flags)
		:AbstractFS(service, mountPoint, flags), m_Source(source)
	{
	}
	MirrorFS::~MirrorFS()
	{
	}

	FSError MirrorFS::Mount()
	{
		if (!m_Source.empty())
		{
			if (IsUsingAsyncIO())
			{
				if (!InitializeAsyncIO())
				{
					return DOKAN_ERROR;
				}
			}

			InitializeMirrorFileHandles();
			return AbstractFS::Mount();
		}
		return DOKAN_ERROR;
	}
}

namespace KxVFS
{
	KxVFS::KxDynamicStringRefW MirrorFS::GetSource() const
	{
		return m_Source;
	}
	void MirrorFS::SetSource(KxDynamicStringRefW source)
	{
		SetOptionIfNotMounted(m_Source, source);
	}

	bool MirrorFS::IsUsingAsyncIO() const
	{
		return m_IsUnsingAsyncIO;
	}
	void MirrorFS::UseAsyncIO(bool value)
	{
		SetOptionIfNotMounted(m_IsUnsingAsyncIO, value);
	}

	bool MirrorFS::IsSecurityFunctionsEnabled() const
	{
		return m_EnableSecurityFunctions;
	}
	void MirrorFS::EnableSecurityFunctions(bool value)
	{
		SetOptionIfNotMounted(m_EnableSecurityFunctions, value);
	}

	bool MirrorFS::ShouldImpersonateCallerUser() const
	{
		return m_ShouldImpersonateCallerUser;
	}
	void MirrorFS::SetImpersonateCallerUser(bool value)
	{
		SetOptionIfNotMounted(m_ShouldImpersonateCallerUser, value);
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
		if (IsUsingAsyncIO())
		{
			CleanupAsyncIO();
		}

		return STATUS_NOT_SUPPORTED;
	}

	NTSTATUS MirrorFS::OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo)
	{
		if (!::GetDiskFreeSpaceExW(m_Source.data(), (ULARGE_INTEGER*)&eventInfo.FreeBytesAvailable, (ULARGE_INTEGER*)&eventInfo.TotalNumberOfBytes, (ULARGE_INTEGER*)&eventInfo.TotalNumberOfFreeBytes))
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	NTSTATUS MirrorFS::OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo)
	{
		eventInfo.VolumeInfo->VolumeSerialNumber = GetVolumeSerialNumber();
		eventInfo.VolumeInfo->SupportsObjects = FALSE;

		const KxDynamicStringW volumeLabel = GetVolumeLabel();
		const size_t labelLength = std::min<size_t>(volumeLabel.length(), eventInfo.MaxLabelLengthInChars);
		eventInfo.VolumeInfo->VolumeLabelLength = WriteString(volumeLabel.data(), eventInfo.VolumeInfo->VolumeLabel, labelLength);

		return STATUS_SUCCESS;
	}
	NTSTATUS MirrorFS::OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo)
	{
		const size_t maxFileSystemNameLengthInBytes = eventInfo.MaxFileSystemNameLengthInChars * sizeof(wchar_t);

		eventInfo.Attributes->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH|FILE_CASE_PRESERVED_NAMES|FILE_SUPPORTS_REMOTE_STORAGE|FILE_UNICODE_ON_DISK|FILE_PERSISTENT_ACLS|FILE_NAMED_STREAMS;
		eventInfo.Attributes->MaximumComponentNameLength = 256; // Not 255, this means Dokan doesn't support long file names?

		wchar_t volumeRoot[4];
		volumeRoot[0] = m_Source[0];
		volumeRoot[1] = L':';
		volumeRoot[2] = L'\\';
		volumeRoot[3] = L'\0';

		DWORD fileSystemFlags = 0;
		DWORD maximumComponentLength = 0;

		wchar_t fileSystemNameBuffer[255] = {0};
		if (::GetVolumeInformationW(volumeRoot, nullptr, 0, nullptr, &maximumComponentLength, &fileSystemFlags, fileSystemNameBuffer, std::size(fileSystemNameBuffer)))
		{
			eventInfo.Attributes->MaximumComponentNameLength = maximumComponentLength;
			eventInfo.Attributes->FileSystemAttributes &= fileSystemFlags;
		}
		eventInfo.Attributes->FileSystemNameLength = WriteString(fileSystemNameBuffer, eventInfo.Attributes->FileSystemName, eventInfo.MaxFileSystemNameLengthInChars);

		return STATUS_SUCCESS;
	}

	NTSTATUS MirrorFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		DWORD errorCode = 0;
		NTSTATUS statusCode = STATUS_SUCCESS;

		KxDynamicStringW targetPath;
		DispatchLocationRequest(eventInfo.FileName, targetPath);

		auto[fileAttributesAndFlags, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);

		// When filePath is a directory, needs to change the flag so that the file can be opened.
		FileAttributes fileAttributes = FromInt<FileAttributes>(::GetFileAttributesW(targetPath));
		if (fileAttributes != FileAttributes::Invalid)
		{
			if (ToBool(fileAttributes & FileAttributes::Directory))
			{
				if (!(eventInfo.CreateOptions & FILE_NON_DIRECTORY_FILE))
				{
					// Needed by FindFirstFile to list files in it
					// TODO: use ReOpenFile in 'OnFindFiles' to set share read temporary
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

		if (IsUsingAsyncIO())
		{
			fileAttributesAndFlags |= FileAttributesAndFlags::FlagOverlapped;
		}

		// This is for Impersonate Caller User Option
		TokenHandle userTokenHandle = ImpersonateCallerUserIfNeeded(eventInfo);

		// Security
		SECURITY_ATTRIBUTES securityAttributes = {0};
		Utility::SecurityObject newFileSecurity = CreateSecurityIfNeeded(eventInfo, securityAttributes, targetPath, creationDisposition);

		if (eventInfo.DokanFileInfo->IsDirectory)
		{
			// It is a create directory request
			if (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::OpenAlways)
			{
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// We create folder
				if (!CreateDirectoryW(targetPath, &securityAttributes))
				{
					errorCode = GetLastError();

					// Fail to create folder for OPEN_ALWAYS is not an error
					if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CreationDisposition::CreateNew)
					{
						statusCode = GetNtStatusByWin32ErrorCode(errorCode);
					}
				}

				CleanupImpersonateCallerUserIfNeeded(userTokenHandle, statusCode);
			}

			if (statusCode == STATUS_SUCCESS)
			{
				// Check first if we're trying to open a file as a directory.
				if (fileAttributes != FileAttributes::Invalid && !ToBool(fileAttributes & FileAttributes::Directory) && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
				{
					return STATUS_NOT_A_DIRECTORY;
				}
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
				Utility::FileHandle fileHandle = CreateFileW(targetPath,
															 ToInt(genericDesiredAccess),
															 eventInfo.ShareAccess,
															 &securityAttributes,
															 OPEN_EXISTING,
															 ToInt(fileAttributesAndFlags|FileAttributesAndFlags::FlagBackupSemantics),
															 nullptr
				);
				CleanupImpersonateCallerUserIfNeeded(userTokenHandle);

				if (fileHandle.IsOK())
				{
					Mirror::FileContext* fileContext = PopMirrorFileHandle(fileHandle);
					if (!fileContext)
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
					SaveFileContext(eventInfo, fileContext);

					// Open succeed but we need to inform the driver that the dir open and not created by returning STATUS_OBJECT_NAME_COLLISION
					if (creationDisposition == CreationDisposition::OpenAlways && fileAttributes != FileAttributes::Invalid)
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
			if (m_EnableSecurityFunctions)
			{
				OpenWithSecurityAccess(genericDesiredAccess, IsWriteRequest(targetPath, genericDesiredAccess, creationDisposition));
			}

			if (!CheckAttributesToOverwriteFile(FromInt<FileAttributes>(fileAttributes), fileAttributesAndFlags, creationDisposition))
			{
				statusCode = STATUS_ACCESS_DENIED;
			}
			else
			{
				// Truncate should always be used with write access
				if (creationDisposition == CreationDisposition::TruncateExisting)
				{
					genericDesiredAccess |= AccessRights::GenericWrite;
				}
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				KxVFSDebugPrint(L"Trying to create/open file: %s", targetPath.data());
				Utility::FileHandle fileHandle = CreateFileW(targetPath,
															 ToInt(genericDesiredAccess),
															 eventInfo.ShareAccess,
															 &securityAttributes,
															 ToInt(creationDisposition),
															 ToInt(fileAttributesAndFlags),
															 nullptr
				);

				CleanupImpersonateCallerUserIfNeeded(userTokenHandle);
				errorCode = GetLastError();

				if (!fileHandle.IsOK())
				{
					KxVFSDebugPrint(L"Failed to create/open file: %s", targetPath.data());
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
				}
				else
				{
					// Need to update FileAttributes with previous when Overwrite file
					if (fileAttributes != FileAttributes::Invalid && creationDisposition == CreationDisposition::TruncateExisting)
					{
						::SetFileAttributesW(targetPath, ToInt(fileAttributesAndFlags) | ToInt(fileAttributes));
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
						SaveFileContext(eventInfo, mirrorContext);

						if (creationDisposition == CreationDisposition::OpenAlways || creationDisposition == CreationDisposition::CreateAlways)
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
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (CriticalSectionLocker lock(fileContext->m_Lock); true)
			{
				fileContext->m_IsClosed = true;
				if (fileContext->m_FileHandle && fileContext->m_FileHandle != INVALID_HANDLE_VALUE)
				{
					CloseHandle(fileContext->m_FileHandle);
					fileContext->m_FileHandle = nullptr;

					KxDynamicStringW targetPath;
					DispatchLocationRequest(eventInfo.FileName, targetPath);
					if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
					{
						OnFileClosed(eventInfo, targetPath);
					}
				}
			}

			eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(nullptr);
			PushMirrorFileHandle(fileContext);
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
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (CriticalSectionLocker lock(fileContext->m_Lock); true)
			{
				CloseHandle(fileContext->m_FileHandle);

				fileContext->m_FileHandle = nullptr;
				fileContext->m_IsCleanedUp = true;

				KxDynamicStringW targetPath;
				DispatchLocationRequest(eventInfo.FileName, targetPath);
				if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
				{
					OnFileCleanedUp(eventInfo, targetPath);
				}
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			//KxDynamicStringW targetPathOld;
			//DispatchLocationRequest(eventInfo.FileName, targetPathOld);

			KxDynamicStringW targetPathNew;
			DispatchLocationRequest(eventInfo.NewFileName, targetPathNew);

			// The FILE_RENAME_INFO struct has space for one WCHAR for the name at the end, so that accounts for the null terminator
			const size_t bufferSize = sizeof(FILE_RENAME_INFO) + targetPathNew.length() * sizeof(targetPathNew[0]);
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

			const BOOL result = SetFileInformationByHandle(fileContext->m_FileHandle, FileRenameInfo, renameInfo, bufferSize);
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
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			BY_HANDLE_FILE_INFORMATION fileInfo = {0};
			if (!::GetFileInformationByHandle(fileContext->m_FileHandle, &fileInfo))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
			{
				return STATUS_CANNOT_DELETE;
			}

			if (eventInfo.DokanFileInfo->IsDirectory)
			{
				KxDynamicStringW targetPath;
				DispatchLocationRequest(eventInfo.FileName, targetPath);
				
				const NTSTATUS status = CanDeleteDirectory(targetPath);
				if (status == STATUS_SUCCESS)
				{
					OnDirectoryDeleted(eventInfo, targetPath);
				}
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}

	NTSTATUS MirrorFS::OnLockFile(EvtLockFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			LARGE_INTEGER length;
			length.QuadPart = eventInfo.Length;

			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.ByteOffset;

			if (!::LockFile(fileContext->m_FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	NTSTATUS MirrorFS::OnUnlockFile(EvtUnlockFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			LARGE_INTEGER length;
			length.QuadPart = eventInfo.Length;

			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.ByteOffset;

			if (!::UnlockFile(fileContext->m_FileHandle, offset.LowPart, offset.HighPart, length.LowPart, length.HighPart))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_SUCCESS;
		}
		return STATUS_INVALID_HANDLE;
	}
	
	NTSTATUS MirrorFS::OnGetFileSecurity(EvtGetFileSecurity& eventInfo)
	{
		if (!IsSecurityFunctionsEnabled())
		{
			return STATUS_NOT_IMPLEMENTED;
		}
		else
		{
			if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
			{
				const bool success = ::GetKernelObjectSecurity(fileContext->m_FileHandle,
															   eventInfo.SecurityInformation,
															   eventInfo.SecurityDescriptor,
															   eventInfo.SecurityDescriptorSize,
															   &eventInfo.LengthNeeded
				);
				if (!success)
				{
					const DWORD error = ::GetLastError();
					if (error == ERROR_INSUFFICIENT_BUFFER)
					{
						KxVFSDebugPrint(L"GetKernelObjectSecurity error: ERROR_INSUFFICIENT_BUFFER\n");
						return STATUS_BUFFER_OVERFLOW;
					}
					else
					{
						KxVFSDebugPrint(L"GetKernelObjectSecurity error: %u\n", error);
						return GetNtStatusByWin32ErrorCode(error);
					}
				}

				// Ensure the Security Descriptor Length is set
				const DWORD securityDescriptorLength = ::GetSecurityDescriptorLength(eventInfo.SecurityDescriptor);
				KxVFSDebugPrint(L"GetKernelObjectSecurity return true, 'eventInfo.LengthNeeded = %u, securityDescriptorLength = %u", eventInfo.LengthNeeded, securityDescriptorLength);
				eventInfo.LengthNeeded = securityDescriptorLength;

				return STATUS_SUCCESS;
			}
			return STATUS_FILE_CLOSED;
		}
	}
	NTSTATUS MirrorFS::OnSetFileSecurity(EvtSetFileSecurity& eventInfo)
	{
		if (!IsSecurityFunctionsEnabled())
		{
			return STATUS_NOT_IMPLEMENTED;
		}
		else
		{
			if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
			{
				if (!::SetKernelObjectSecurity(fileContext->m_FileHandle, eventInfo.SecurityInformation, eventInfo.SecurityDescriptor))
				{
					const DWORD error = ::GetLastError();
					KxVFSDebugPrint(L"SetKernelObjectSecurity error: %u\n", error);
					return GetNtStatusByWin32ErrorCode(error);
				}
				return STATUS_SUCCESS;
			}
			return STATUS_FILE_CLOSED;
		}
	}

	NTSTATUS MirrorFS::OnReadFile(EvtReadFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isCleanedUp = false;
			bool isClosed = false;
			GetMirrorFileHandleState(fileContext, &isCleanedUp, &isClosed);

			if (isClosed)
			{
				return STATUS_FILE_CLOSED;
			}

			if (isCleanedUp)
			{
				KxDynamicStringW targetPath;
				DispatchLocationRequest(eventInfo.FileName, targetPath);

				Utility::FileHandle tempFileHandle = ::CreateFileW(targetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
				if (tempFileHandle.IsOK())
				{
					const NTSTATUS status = ReadFileSync(eventInfo, tempFileHandle);
					OnFileRead(eventInfo, targetPath);
					return status;
				}
				return GetNtStatusByWin32LastErrorCode();
			}

			if (IsUsingAsyncIO())
			{
				Mirror::OverlappedContext* overlappedContext = PopMirrorOverlapped();
				if (!overlappedContext)
				{
					return STATUS_MEMORY_NOT_ALLOCATED;
				}

				Utility::Int64ToOverlappedOffset(eventInfo.Offset, overlappedContext->m_InternalOverlapped);
				overlappedContext->m_FileContext = fileContext;
				overlappedContext->m_Context = &eventInfo;
				overlappedContext->m_IOType = Mirror::IOOperationType::Read;

				// Async operation, call before read
				OnFileRead(eventInfo, KxNullDynamicStringW);

				StartThreadpoolIo(fileContext->m_IOCompletion);
				if (!::ReadFile(fileContext->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToRead, &eventInfo.NumberOfBytesRead, &overlappedContext->m_InternalOverlapped))
				{
					DWORD errorCode = GetLastError();
					if (errorCode != ERROR_IO_PENDING)
					{
						::CancelThreadpoolIo(fileContext->m_IOCompletion);
						return GetNtStatusByWin32ErrorCode(errorCode);
					}
				}
				return STATUS_PENDING;
			}
			else
			{
				const NTSTATUS status = ReadFileSync(eventInfo, fileContext->m_FileHandle);;
				OnFileRead(eventInfo, KxNullDynamicStringW);
				return status;
			}
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS MirrorFS::OnWriteFile(EvtWriteFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isCleanedUp = false;
			bool isClosed = false;
			GetMirrorFileHandleState(fileContext, &isCleanedUp, &isClosed);
			if (isClosed)
			{
				return STATUS_FILE_CLOSED;
			}

			uint64_t fileSize = 0;
			if (isCleanedUp)
			{
				KxDynamicStringW targetPath;
				DispatchLocationRequest(eventInfo.FileName, targetPath);

				Utility::FileHandle tempFileHandle = ::CreateFileW(targetPath, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
				if (!tempFileHandle.IsOK())
				{
					return GetNtStatusByWin32LastErrorCode();
				}

				LARGE_INTEGER liFileSize = {0};
				if (!::GetFileSizeEx(fileContext->m_FileHandle, &liFileSize))
				{
					return GetNtStatusByWin32LastErrorCode();
				}
				fileSize = liFileSize.QuadPart;

				// Need to check if its really needs to be 'mirrorContext->m_FileHandle' and not 'tempFileHandle'.
				NTSTATUS status = WriteFileSync(eventInfo, fileContext->m_FileHandle, fileSize);
				OnFileWritten(eventInfo, targetPath);
				return status;
			}

			LARGE_INTEGER liFileSize = {0};
			if (!::GetFileSizeEx(fileContext->m_FileHandle, &liFileSize))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			fileSize = liFileSize.QuadPart;

			if (IsUsingAsyncIO())
			{
				// Paging IO, I need to read about it at some point. Until then, don't touch this code.
				if (eventInfo.DokanFileInfo->PagingIo)
				{
					if ((uint64_t)eventInfo.Offset >= fileSize)
					{
						eventInfo.NumberOfBytesWritten = 0;
						return STATUS_SUCCESS;
					}

					if (((uint64_t)eventInfo.Offset + eventInfo.NumberOfBytesToWrite) > fileSize)
					{
						uint64_t bytes = fileSize - eventInfo.Offset;
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

				Mirror::OverlappedContext* overlappedContext = PopMirrorOverlapped();
				if (!overlappedContext)
				{
					return STATUS_MEMORY_NOT_ALLOCATED;
				}

				Utility::Int64ToOverlappedOffset(eventInfo.Offset, overlappedContext->m_InternalOverlapped);
				overlappedContext->m_FileContext = fileContext;
				overlappedContext->m_Context = &eventInfo;
				overlappedContext->m_IOType = Mirror::IOOperationType::Write;

				// Call here, because it's async operation
				OnFileWritten(eventInfo, KxNullDynamicStringW);

				StartThreadpoolIo(fileContext->m_IOCompletion);
				if (!::WriteFile(fileContext->m_FileHandle, eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, &eventInfo.NumberOfBytesWritten, &overlappedContext->m_InternalOverlapped))
				{
					DWORD errorCode = ::GetLastError();
					if (errorCode != ERROR_IO_PENDING)
					{
						::CancelThreadpoolIo(fileContext->m_IOCompletion);
						return GetNtStatusByWin32ErrorCode(errorCode);
					}
				}
				return STATUS_PENDING;
			}
			else
			{
				NTSTATUS status = WriteFileSync(eventInfo, fileContext->m_FileHandle, fileSize);
				OnFileWritten(eventInfo, KxNullDynamicStringW);
				return status;
			}
		}
		return STATUS_FILE_CLOSED;
	}
	
	NTSTATUS MirrorFS::OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (::FlushFileBuffers(fileContext->m_FileHandle))
			{
				OnFileBuffersFlushed(eventInfo, KxNullDynamicStringW);
				return STATUS_SUCCESS;
			}
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS MirrorFS::OnSetEndOfFile(EvtSetEndOfFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			LARGE_INTEGER offset;
			offset.QuadPart = eventInfo.Length;

			if (!::SetFilePointerEx(fileContext->m_FileHandle, offset, nullptr, FILE_BEGIN))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if (!::SetEndOfFile(fileContext->m_FileHandle))
			{
				return GetNtStatusByWin32LastErrorCode();
			}

			OnEndOfFileSet(eventInfo, KxNullDynamicStringW);
			return STATUS_SUCCESS;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS MirrorFS::OnSetAllocationSize(EvtSetAllocationSize& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			LARGE_INTEGER fileSize;
			if (::GetFileSizeEx(fileContext->m_FileHandle, &fileSize))
			{
				if (eventInfo.Length < fileSize.QuadPart)
				{
					fileSize.QuadPart = eventInfo.Length;

					if (!::SetFilePointerEx(fileContext->m_FileHandle, fileSize, nullptr, FILE_BEGIN))
					{
						return GetNtStatusByWin32LastErrorCode();
					}
					if (!::SetEndOfFile(fileContext->m_FileHandle))
					{
						return GetNtStatusByWin32LastErrorCode();
					}
				}
			}
			else
			{
				return GetNtStatusByWin32LastErrorCode();
			}

			OnAllocationSizeSet(eventInfo, KxNullDynamicStringW);
			return STATUS_SUCCESS;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS MirrorFS::OnGetFileInfo(EvtGetFileInfo& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (!::GetFileInformationByHandle(fileContext->m_FileHandle, &eventInfo.FileHandleInfo))
			{
				KxDynamicStringW targetPath;
				DispatchLocationRequest(eventInfo.FileName, targetPath);
				KxVFSDebugPrint(L"Couldn't get file info by handle, trying by file name: %s", targetPath.data());

				// FileName is a root directory, in this case, 'FindFirstFile' can't get directory information.
				if (wcslen(eventInfo.FileName) == 1)
				{
					eventInfo.FileHandleInfo.dwFileAttributes = ::GetFileAttributesW(targetPath);
				}
				else
				{
					WIN32_FIND_DATAW findData = {0};
					HANDLE fileHandle = ::FindFirstFileW(targetPath, &findData);
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
					::FindClose(fileHandle);
				}
			}

			KxVFSDebugPrint(L"Successfully retrieved file info by handle: %s", eventInfo.FileName);
			return STATUS_SUCCESS;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS MirrorFS::OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (!::SetFileInformationByHandle(fileContext->m_FileHandle, FileBasicInfo, eventInfo.Info, (DWORD)sizeof(Dokany2::FILE_BASIC_INFORMATION)))
			{
				return GetNtStatusByWin32LastErrorCode();
			}

			OnBasicFileInfoSet(eventInfo, KxNullDynamicStringW);
			return STATUS_SUCCESS;
		}
		return STATUS_FILE_CLOSED;
	}

	NTSTATUS MirrorFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		KxDynamicStringW targetPath;
		DispatchLocationRequest(eventInfo.PathName, targetPath);
		size_t targetPathLength = targetPath.length();

		auto AppendAsterix = [](KxDynamicStringW& path)
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
		AppendAsterix(targetPath);

		WIN32_FIND_DATAW findData = {0};
		HANDLE findHandle = ::FindFirstFileW(targetPath, &findData);

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
		while (::FindNextFileW(findHandle, &findData) != 0);

		DWORD errorCode = GetLastError();
		::FindClose(findHandle);

		if (errorCode != ERROR_NO_MORE_FILES)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
	NTSTATUS MirrorFS::OnFindStreams(EvtFindStreams& eventInfo)
	{
		KxDynamicStringW targetPath;
		DispatchLocationRequest(eventInfo.FileName, targetPath);

		WIN32_FIND_STREAM_DATA findData = {0};
		HANDLE findHandle = ::FindFirstStreamW(targetPath, FindStreamInfoStandard, &findData, 0);
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
			while (::FindNextStreamW(findHandle, &findData) != 0 && (findResult = eventInfo.FillFindStreamData(&eventInfo, &findData)) == Dokany2::DOKAN_STREAM_BUFFER_CONTINUE)
			{
				count++;
			}
		}

		errorCode = GetLastError();
		::FindClose(findHandle);

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
	void MirrorFS::FreeMirrorFileHandle(Mirror::FileContext* fileContext)
	{
		if (fileContext)
		{
			if (IsUsingAsyncIO() && fileContext->m_IOCompletion)
			{
				if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup && fileContext->m_IOCompletion)
				{
					::CloseThreadpoolIo(fileContext->m_IOCompletion);
					fileContext->m_IOCompletion = nullptr;
				}
			}
			delete fileContext;
		}
	}
	void MirrorFS::PushMirrorFileHandle(Mirror::FileContext* fileContext)
	{
		if (IsUsingAsyncIO() && fileContext->m_IOCompletion)
		{
			if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
			{
				CloseThreadpoolIo(fileContext->m_IOCompletion);
			}
			fileContext->m_IOCompletion = nullptr;
		}

		if (CriticalSectionLocker lock(m_FileHandlePoolCS); true)
		{
			DokanVector_PushBack(&m_FileHandlePool, &fileContext);
		}
	}
	Mirror::FileContext* MirrorFS::PopMirrorFileHandle(HANDLE fileHandle)
	{
		if (fileHandle == nullptr || fileHandle == INVALID_HANDLE_VALUE || InterlockedAdd(&m_IsUnmounted, 0) != FALSE)
		{
			return nullptr;
		}

		Mirror::FileContext* mirrorContext = nullptr;
		if (CriticalSectionLocker lock(m_FileHandlePoolCS); true)
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
		mirrorContext->m_FileHandle = fileHandle;
		mirrorContext->m_IsCleanedUp = false;
		mirrorContext->m_IsClosed = false;

		if (IsUsingAsyncIO())
		{
			if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
			{
				mirrorContext->m_IOCompletion = ::CreateThreadpoolIo(fileHandle, MirrorIoCallback, mirrorContext, &m_ThreadPoolEnvironment);
			}

			if (!mirrorContext->m_IOCompletion)
			{
				PushMirrorFileHandle(mirrorContext);
				mirrorContext = nullptr;
			}
		}
		return mirrorContext;
	}
	void MirrorFS::GetMirrorFileHandleState(Mirror::FileContext* fileHandle, bool* isCleanedUp, bool* isClosed) const
	{
		if (fileHandle && (isCleanedUp || isClosed))
		{
			if (CriticalSectionLocker lock(fileHandle->m_Lock); true)
			{
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
	}

	bool MirrorFS::InitializeMirrorFileHandles()
	{
		return DokanVector_StackAlloc(&m_FileHandlePool, sizeof(Mirror::FileContext*));
	}
	void MirrorFS::CleanupMirrorFileHandles()
	{
		if (CriticalSectionLocker lock(m_FileHandlePoolCS); true)
		{
			for (size_t i = 0; i < DokanVector_GetCount(&m_FileHandlePool); ++i)
			{
				FreeMirrorFileHandle(*(Mirror::FileContext**)DokanVector_GetItem(&m_FileHandlePool, i));
			}
			DokanVector_Free(&m_FileHandlePool);
		}
	}
}

namespace KxVFS
{
	void MirrorFS::FreeMirrorOverlapped(Mirror::OverlappedContext* overlappedContext) const
	{
		delete overlappedContext;
	}
	void MirrorFS::PushMirrorOverlapped(Mirror::OverlappedContext* overlappedContext)
	{
		if (CriticalSectionLocker lock(m_OverlappedPoolCS); true)
		{
			DokanVector_PushBack(&m_OverlappedPool, &overlappedContext);
		}
	}
	Mirror::OverlappedContext* MirrorFS::PopMirrorOverlapped()
	{
		Mirror::OverlappedContext* overlappedContext = nullptr;
		if (CriticalSectionLocker lock(m_OverlappedPoolCS); true)
		{
			if (DokanVector_GetCount(&m_OverlappedPool) > 0)
			{
				overlappedContext = *(Mirror::OverlappedContext**)DokanVector_GetLastItem(&m_OverlappedPool);
				DokanVector_PopBack(&m_OverlappedPool);
			}
		}

		if (!overlappedContext)
		{
			overlappedContext = new Mirror::OverlappedContext();
		}
		return overlappedContext;
	}

	bool MirrorFS::InitializeAsyncIO()
	{
		m_ThreadPool = Dokany2::DokanGetThreadPool();
		if (!m_ThreadPool)
		{
			return false;
		}

		m_ThreadPoolCleanupGroup = ::CreateThreadpoolCleanupGroup();
		if (!m_ThreadPoolCleanupGroup)
		{
			return false;
		}

		::InitializeThreadpoolEnvironment(&m_ThreadPoolEnvironment);

		::SetThreadpoolCallbackPool(&m_ThreadPoolEnvironment, m_ThreadPool);
		::SetThreadpoolCallbackCleanupGroup(&m_ThreadPoolEnvironment, m_ThreadPoolCleanupGroup, nullptr);

		DokanVector_StackAlloc(&m_OverlappedPool, sizeof(Mirror::OverlappedContext*));
		return true;
	}
	void MirrorFS::CleanupPendingAsyncIO()
	{
		if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
		{
			::CloseThreadpoolCleanupGroupMembers(m_ThreadPoolCleanupGroup, FALSE, nullptr);
			::CloseThreadpoolCleanupGroup(m_ThreadPoolCleanupGroup);
			m_ThreadPoolCleanupGroup = nullptr;

			::DestroyThreadpoolEnvironment(&m_ThreadPoolEnvironment);
		}
	}
	void MirrorFS::CleanupAsyncIO()
	{
		CleanupPendingAsyncIO();

		if (CriticalSectionLocker lock(m_OverlappedPoolCS); true)
		{
			for (size_t i = 0; i < DokanVector_GetCount(&m_OverlappedPool); ++i)
			{
				FreeMirrorOverlapped(*(Mirror::OverlappedContext**)DokanVector_GetItem(&m_OverlappedPool, i));
			}
			DokanVector_Free(&m_OverlappedPool);
		}
	}
	void CALLBACK MirrorFS::MirrorIoCallback(PTP_CALLBACK_INSTANCE instance,
											 PVOID context,
											 PVOID overlapped,
											 ULONG resultIO,
											 ULONG_PTR numberOfBytesTransferred,
											 PTP_IO portIO
	)
	{
		UNREFERENCED_PARAMETER(instance);
		UNREFERENCED_PARAMETER(portIO);

		Mirror::FileContext* mirrorContext = reinterpret_cast<Mirror::FileContext*>(context);
		Mirror::OverlappedContext* overlappedContext = reinterpret_cast<Mirror::OverlappedContext*>(overlapped);
		EvtReadFile* readFileEvent = nullptr;
		EvtWriteFile* writeFileEvent = nullptr;

		switch (overlappedContext->m_IOType)
		{
			case Mirror::IOOperationType::Read:
			{
				readFileEvent = (EvtReadFile*)overlappedContext->m_Context;
				readFileEvent->NumberOfBytesRead = (DWORD)numberOfBytesTransferred;
				DokanEndDispatchRead(readFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
			case Mirror::IOOperationType::Write:
			{
				writeFileEvent = (EvtWriteFile*)overlappedContext->m_Context;
				writeFileEvent->NumberOfBytesWritten = (DWORD)numberOfBytesTransferred;
				DokanEndDispatchWrite(writeFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
		};
		mirrorContext->GetFSInstance()->PushMirrorOverlapped(overlappedContext);
	}
}
