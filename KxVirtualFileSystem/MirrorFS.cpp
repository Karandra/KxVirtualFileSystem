#include "stdafx.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/Utility.h"
#include "MirrorFS.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

namespace KxVFS
{
	bool MirrorFS::CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, DynamicStringRefW filePath) const
	{
		if (fileInfo->DeleteOnClose)
		{
			KxVFS_Log(LogLevel::Info, L"%1: \"%2\"", __FUNCTIONW__, filePath);

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
	NtStatus MirrorFS::CanDeleteDirectory(DynamicStringRefW directoryPath) const
	{
		if (!directoryPath.empty())
		{
			if (FileFinder::IsDirectoryEmpty(directoryPath))
			{
				return NtStatus::Success;
			}
			else
			{
				return NtStatus::DirectoryNotEmpty;
			}
		}
		return NtStatus::ObjectPathInvalid;
	}

	DynamicStringW MirrorFS::DispatchLocationRequest(DynamicStringRefW requestedPath)
	{
		DynamicStringW targetPath = Utility::GetLongPathPrefix();
		targetPath += m_Source;
		targetPath += requestedPath;

		return targetPath;
	}
}

namespace KxVFS
{
	MirrorFS::MirrorFS(FileSystemService& service, DynamicStringRefW mountPoint, DynamicStringRefW source, FSFlags flags)
		:DokanyFileSystem(service, mountPoint, flags), ExtendedSecurity(this), CallerUserImpersonation(this), m_Source(source)
	{
	}

	DynamicStringRefW MirrorFS::GetSource() const
	{
		return m_Source;
	}
	void MirrorFS::SetSource(DynamicStringRefW source)
	{
		m_Source = source;
	}

	NtStatus MirrorFS::GetVolumeSizeInfo(int64_t& freeBytes, int64_t& totalSize)
	{
		KxVFS_Log(LogLevel::Info, L"Attempt to get volume size info for: %1", m_Source);

		if (::GetDiskFreeSpaceExW(m_Source, nullptr, reinterpret_cast<ULARGE_INTEGER*>(&totalSize), reinterpret_cast<ULARGE_INTEGER*>(&freeBytes)))
		{
			// This returns a bit different size than Explorer shows for real disk.
			// Might not be a good idea to use it here.
			#if 0
			// Try to get real disk length using DeviceIoControl
			FileHandle device;
			DWORD returnedBytes = 0;
			GET_LENGTH_INFORMATION lengthInfo = {};
			if (device.OpenVolumeDevice(m_Source[0]) && ::DeviceIoControl(device, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo, sizeof(lengthInfo), &returnedBytes, nullptr))
			{
				totalSize = lengthInfo.Length.QuadPart;
			}
			else
			{
				KxVFS_Log(LogLevel::Info, L"Unable get disk size info using 'DeviceIoControl', total size might be incorrect");
			}
			#endif
			return NtStatus::Success;
		}
		else
		{
			KxVFS_Log(LogLevel::Info, L"Unable get disk size info: %1", m_Source.data());
			return GetNtStatusByWin32LastErrorCode();
		}
	}
}

namespace KxVFS
{
	NtStatus MirrorFS::OnMount(EvtMounted& eventInfo)
	{
		return NtStatus::Success;
	}
	NtStatus MirrorFS::OnUnMount(EvtUnMounted& eventInfo)
	{
		return NtStatus::Success;
	}

	NtStatus MirrorFS::OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo)
	{
		int64_t freeBytes = 0;
		int64_t totalSize = 0;
		const NtStatus status = GetVolumeSizeInfo(freeBytes, totalSize);

		eventInfo.FreeBytesAvailable = freeBytes;
		eventInfo.TotalNumberOfFreeBytes = freeBytes;
		eventInfo.TotalNumberOfBytes = totalSize;
		return status;
	}
	NtStatus MirrorFS::OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo)
	{
		eventInfo.VolumeInfo->VolumeSerialNumber = GetVolumeSerialNumber();
		eventInfo.VolumeInfo->SupportsObjects = FALSE;

		const DynamicStringW volumeLabel = GetVolumeLabel();
		const size_t labelLength = std::min<size_t>(volumeLabel.length(), eventInfo.MaxLabelLengthInChars);
		eventInfo.VolumeInfo->VolumeLabelLength = Utility::WriteString(volumeLabel.data(), eventInfo.VolumeInfo->VolumeLabel, labelLength);

		return NtStatus::Success;
	}
	NtStatus MirrorFS::OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo)
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
		eventInfo.Attributes->FileSystemNameLength = Utility::WriteString(fileSystemNameBuffer, eventInfo.Attributes->FileSystemName, eventInfo.MaxFileSystemNameLengthInChars);

		return NtStatus::Success;
	}

	NtStatus MirrorFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"Trying to create/open file or directory: %1", eventInfo.FileName);

		DWORD errorCode = 0;
		NtStatus statusCode = NtStatus::Success;
		DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);

		const FileShare fileShareOptions = FromInt<FileShare>(eventInfo.ShareAccess);
		auto[requestAttributes, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);

		// When filePath is a directory, needs to change the flag so that the file can be opened.
		const FileAttributes fileAttributes = FromInt<FileAttributes>(::GetFileAttributesW(targetPath));
		if (fileAttributes != FileAttributes::Invalid)
		{
			if (fileAttributes & FileAttributes::Directory)
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
					return NtStatus::FileIsADirectory;
				}
			}
		}

		IOManager& ioManager = GetIOManager();
		if (GetIOManager().IsAsyncIOEnabled())
		{
			requestAttributes |= FileAttributes::FlagOverlapped;
		}

		TokenHandle userTokenHandle = ImpersonateCallerUserIfEnabled(eventInfo);
		SecurityObject newFileSecurity = CreateSecurityIfEnabled(eventInfo, targetPath, creationDisposition);

		if (eventInfo.DokanFileInfo->IsDirectory)
		{
			// It is a create directory request
			if (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::OpenAlways)
			{
				ImpersonateLoggedOnUserIfEnabled(userTokenHandle);

				// Create folder
				if (!Utility::CreateDirectory(targetPath, &newFileSecurity.GetAttributes()))
				{
					errorCode = GetLastError();

					// Fail to create folder for OPEN_ALWAYS is not an error
					if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CreationDisposition::CreateNew)
					{
						statusCode = GetNtStatusByWin32ErrorCode(errorCode);
					}
				}

				CleanupImpersonateCallerUserIfEnabled(userTokenHandle, statusCode);
			}

			if (statusCode == NtStatus::Success)
			{
				// Check first if we're trying to open a file as a directory.
				if (fileAttributes != FileAttributes::Invalid && !(fileAttributes & FileAttributes::Directory) && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
				{
					return NtStatus::NotADirectory;
				}

				// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
				ImpersonateLoggedOnUserIfEnabled(userTokenHandle);
				FileHandle directoryHandle(targetPath,
										   genericDesiredAccess,
										   fileShareOptions,
										   CreationDisposition::OpenExisting,
										   requestAttributes|FileAttributes::FlagBackupSemantics,
										   &newFileSecurity.GetAttributes()
				);
				CleanupImpersonateCallerUserIfEnabled(userTokenHandle);

				if (directoryHandle.IsValid())
				{
					FileContextManager& fileContextManager = GetFileContextManager();
					FileContext* fileContext = SaveFileContext(eventInfo, fileContextManager.PopContext(std::move(directoryHandle)));
					if (fileContext)
					{
						fileContext->GetEventInfo().Assign(eventInfo);
						OnFileCreated(eventInfo, *fileContext);
					}
					else
					{
						SetLastError(ERROR_INTERNAL_ERROR);
						statusCode = NtStatus::InternalError;
					}

					// Open succeed but we need to inform the driver that the dir open and not created by returning NtStatus::ObjectNameCollision
					if (creationDisposition == CreationDisposition::OpenAlways && fileAttributes != FileAttributes::Invalid)
					{
						statusCode = NtStatus::ObjectNameCollision;
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
			if (!CheckAttributesToOverwriteFile(FromInt<FileAttributes>(fileAttributes), requestAttributes, creationDisposition))
			{
				statusCode = NtStatus::AccessDenied;
			}
			else
			{
				const bool isWriteRequest = IsWriteRequest(targetPath, genericDesiredAccess, creationDisposition);

				// Truncate should always be used with write access
				if (creationDisposition == CreationDisposition::TruncateExisting)
				{
					genericDesiredAccess |= AccessRights::GenericWrite;
				}
				ImpersonateLoggedOnUserIfEnabled(userTokenHandle);
				OpenWithSecurityAccessIfEnabled(genericDesiredAccess, isWriteRequest);

				KxVFS_Log(LogLevel::Info, L"Trying to create/open file: %1", targetPath);
				FileHandle fileHandle(targetPath,
									  genericDesiredAccess,
									  fileShareOptions,
									  creationDisposition,
									  requestAttributes,
									  &newFileSecurity.GetAttributes()
				);
				CleanupImpersonateCallerUserIfEnabled(userTokenHandle);
				errorCode = GetLastError();

				if (!fileHandle.IsValid())
				{
					KxVFS_Log(LogLevel::Info, L"Failed to create/open file: %1", targetPath);
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
				}
				else
				{
					// Need to update FileAttributes with previous when Overwrite file
					if (fileAttributes != FileAttributes::Invalid && creationDisposition == CreationDisposition::TruncateExisting)
					{
						::SetFileAttributesW(targetPath, ToInt(requestAttributes) | ToInt(fileAttributes));
					}

					FileContextManager& fileContextManager = GetFileContextManager();
					FileContext* fileContext = fileContextManager.PopContext(std::move(fileHandle));
					if (!fileContext)
					{
						SetLastError(ERROR_INTERNAL_ERROR);
						statusCode = NtStatus::InternalError;
					}
					else
					{
						// Save the file handle in m_Context
						SaveFileContext(eventInfo, fileContext);
						fileContext->GetEventInfo().Assign(eventInfo);

						if (creationDisposition == CreationDisposition::OpenAlways || creationDisposition == CreationDisposition::CreateAlways)
						{
							if (errorCode == ERROR_ALREADY_EXISTS)
							{
								// Open succeed but we need to inform the driver,
								// that the file open and not created by returning NtStatus::ObjectNameCollision
								statusCode = NtStatus::ObjectNameCollision;
							}
						}
					}
				}
			}
		}
		return statusCode;
	}
	NtStatus MirrorFS::OnCloseFile(EvtCloseFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (auto lock = fileContext->LockExclusive(); true)
			{
				fileContext->MarkClosed();
				if (fileContext->GetHandle().IsValidNonNull())
				{
					fileContext->CloseHandle();

					DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);
					OnFileClosed(eventInfo, *fileContext);

					if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
					{
						OnFileDeleted(eventInfo, *fileContext);
						fileContext->ResetFileNode();
					}
				}
			}

			ResetFileContext(eventInfo);
			GetFileContextManager().PushContext(*fileContext);
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnCleanUp(EvtCleanUp& eventInfo)
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
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (auto lock = fileContext->LockExclusive(); true)
			{
				fileContext->CloseHandle();
				fileContext->MarkCleanedUp();

				DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);
				OnFileCleanedUp(eventInfo, *fileContext);

				if (CheckDeleteOnClose(eventInfo.DokanFileInfo, targetPath))
				{
					OnFileDeleted(eventInfo, *fileContext);
					fileContext->ResetFileNode();
				}
			}
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			DynamicStringW targetPathNew = DispatchLocationRequest(eventInfo.NewFileName);
			return fileContext->GetHandle().SetPath(targetPathNew, eventInfo.ReplaceIfExists);
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnCanDeleteFile(EvtCanDeleteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			BY_HANDLE_FILE_INFORMATION fileInfo = {0};
			if (!fileContext->GetHandle().GetInfo(fileInfo))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
			{
				return NtStatus::CannotDelete;
			}

			if (eventInfo.DokanFileInfo->IsDirectory)
			{
				DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);
				return CanDeleteDirectory(targetPath);
			}
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}

	NtStatus MirrorFS::OnLockFile(EvtLockFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (!fileContext->GetHandle().Lock(eventInfo.ByteOffset, eventInfo.Length))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnUnlockFile(EvtUnlockFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (!fileContext->GetHandle().Unlock(eventInfo.ByteOffset, eventInfo.Length))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	
	NtStatus MirrorFS::OnGetFileSecurity(EvtGetFileSecurity& eventInfo)
	{
		if (!IsExtendedSecurityEnabled())
		{
			return NtStatus::NotImplemented;
		}
		else
		{
			if (FileContext* fileContext = GetFileContext(eventInfo))
			{
				const bool success = ::GetKernelObjectSecurity(fileContext->GetHandle(),
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
						KxVFS_Log(LogLevel::Info, L"GetKernelObjectSecurity error: ERROR_INSUFFICIENT_BUFFER");
						return NtStatus::BufferOverflow;
					}
					else
					{
						KxVFS_Log(LogLevel::Info, L"GetKernelObjectSecurity error: %1", error);
						return GetNtStatusByWin32ErrorCode(error);
					}
				}

				// Ensure the Security Descriptor Length is set
				const DWORD securityDescriptorLength = ::GetSecurityDescriptorLength(eventInfo.SecurityDescriptor);
				KxVFS_Log(LogLevel::Info, L"GetKernelObjectSecurity return true, 'eventInfo.LengthNeeded = %1, securityDescriptorLength = %2", eventInfo.LengthNeeded, securityDescriptorLength);
				eventInfo.LengthNeeded = securityDescriptorLength;

				return NtStatus::Success;
			}
			return NtStatus::FileClosed;
		}
	}
	NtStatus MirrorFS::OnSetFileSecurity(EvtSetFileSecurity& eventInfo)
	{
		if (!IsExtendedSecurityEnabled())
		{
			return NtStatus::NotImplemented;
		}
		else
		{
			if (FileContext* fileContext = GetFileContext(eventInfo))
			{
				if (!::SetKernelObjectSecurity(fileContext->GetHandle(), eventInfo.SecurityInformation, eventInfo.SecurityDescriptor))
				{
					const DWORD error = ::GetLastError();
					KxVFS_Log(LogLevel::Info, L"SetKernelObjectSecurity error: %1", error);
					return GetNtStatusByWin32ErrorCode(error);
				}
				return NtStatus::Success;
			}
			return NtStatus::FileClosed;
		}
	}

	NtStatus MirrorFS::OnReadFile(EvtReadFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isClosed = false;
			bool isCleanedUp = false;
			fileContext->InterlockedGetState(isClosed, isCleanedUp);

			if (isClosed)
			{
				return NtStatus::FileClosed;
			}
			if (isCleanedUp)
			{
				DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);

				FileHandle tempHandle(targetPath, AccessRights::GenericRead, FileShare::All, CreationDisposition::OpenExisting);
				if (tempHandle.IsValid())
				{
					return GetIOManager().ReadFileSync(tempHandle, eventInfo, fileContext);
				}
				return GetNtStatusByWin32LastErrorCode();
			}

			IOManager& ioManager = GetIOManager();
			if (ioManager.IsAsyncIOEnabled())
			{
				return ioManager.ReadFileAsync(*fileContext, eventInfo);
			}
			else
			{
				return ioManager.ReadFileSync(fileContext->GetHandle(), eventInfo, fileContext);
			}
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnWriteFile(EvtWriteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isClosed = false;
			bool isCleanedUp = false;
			fileContext->InterlockedGetState(isClosed, isCleanedUp);

			if (isClosed)
			{
				return NtStatus::FileClosed;
			}
			if (isCleanedUp)
			{
				DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);

				FileHandle tempHandle(targetPath, AccessRights::GenericWrite, FileShare::All, CreationDisposition::OpenExisting);
				if (tempHandle.IsValid())
				{
					// Need to check if its really needs to be handle of 'fileContext' and not 'tempHandle'.
					return GetIOManager().WriteFileSync(fileContext->GetHandle(), eventInfo, fileContext);
				}
				return GetNtStatusByWin32LastErrorCode();
			}

			IOManager& ioManager = GetIOManager();
			if (ioManager.IsAsyncIOEnabled())
			{
				return ioManager.WriteFileAsync(*fileContext, eventInfo);
			}
			else
			{
				return ioManager.WriteFileSync(fileContext->GetHandle(), eventInfo, fileContext);
			}
		}
		return NtStatus::FileClosed;
	}
	
	NtStatus MirrorFS::OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (fileContext->GetHandle().FlushBuffers())
			{
				OnFileBuffersFlushed(eventInfo, *fileContext);
				return NtStatus::Success;
			}
			return GetNtStatusByWin32LastErrorCode();
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnSetEndOfFile(EvtSetEndOfFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			FileHandle& handle = fileContext->GetHandle();
			if (!handle.Seek(eventInfo.Length, FileSeekMode::Start))
			{
				return GetNtStatusByWin32LastErrorCode();
			}
			if (!handle.SetEnd())
			{
				return GetNtStatusByWin32LastErrorCode();
			}

			OnEndOfFileSet(eventInfo, *fileContext);
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnSetAllocationSize(EvtSetAllocationSize& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			FileHandle& handle = fileContext->GetHandle();
			if (int64_t fileSize = 0; handle.GetFileSize(fileSize))
			{
				if (eventInfo.Length < fileSize)
				{
					fileSize = eventInfo.Length;
					if (!handle.Seek(fileSize, FileSeekMode::Start))
					{
						return GetNtStatusByWin32LastErrorCode();
					}
					if (!handle.SetEnd())
					{
						return GetNtStatusByWin32LastErrorCode();
					}
				}
			}
			else
			{
				return GetNtStatusByWin32LastErrorCode();
			}

			OnAllocationSizeSet(eventInfo, *fileContext);
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnGetFileInfo(EvtGetFileInfo& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (!fileContext->GetHandle().GetInfo(eventInfo.FileHandleInfo))
			{
				DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);
				KxVFS_Log(LogLevel::Info, L"Couldn't get file info by handle, trying by file name: %1", targetPath);

				// FileName is a root directory, in this case, 'FindFirstFile' can't get directory information.
				if (wcslen(eventInfo.FileName) == 1)
				{
					eventInfo.FileHandleInfo.dwFileAttributes = ::GetFileAttributesW(targetPath);
				}
				else
				{
					WIN32_FIND_DATAW findData = {0};
					SearchHandle fileHandle = ::FindFirstFileW(targetPath, &findData);
					if (fileHandle.IsValid())
					{
						eventInfo.FileHandleInfo.dwFileAttributes = findData.dwFileAttributes;
						eventInfo.FileHandleInfo.ftCreationTime = findData.ftCreationTime;
						eventInfo.FileHandleInfo.ftLastAccessTime = findData.ftLastAccessTime;
						eventInfo.FileHandleInfo.ftLastWriteTime = findData.ftLastWriteTime;
						eventInfo.FileHandleInfo.nFileSizeHigh = findData.nFileSizeHigh;
						eventInfo.FileHandleInfo.nFileSizeLow = findData.nFileSizeLow;
					}
					return GetNtStatusByWin32LastErrorCode();
				}
			}

			KxVFS_Log(LogLevel::Info, L"Successfully retrieved file info by handle: %1", eventInfo.FileName);
			return NtStatus::Success;
		}
		return NtStatus::FileClosed;
	}
	NtStatus MirrorFS::OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (auto lock = fileContext->LockExclusive(); true)
			{
				const bool sucess = fileContext->GetHandle().SetInfo(FileBasicInfo, *eventInfo.Info);
				const DWORD errorCode = ::GetLastError();

				// Check error code because this function can be called when async IO is in progress
				// and if we don't check for errors in this case, then we are going to update file node
				// or anything else with invalid attributes in upper layers if they are using 'OnBasicFileInfoSet' event.

				// In case of async IO just return success so any copy operation won't hung file system process,
				// but don't call event handler to avoid behavior described in above comment.
				// This is actually consistent with old implementation from version 1.x which returned 'NtStatus::Success'
				// when set info function itself succeeded but regardless of error code.
				if (sucess)
				{
					if (errorCode == ERROR_SUCCESS)
					{
						OnBasicFileInfoSet(eventInfo, *fileContext);
					}
					return NtStatus::Success;
				}
				return GetNtStatusByWin32ErrorCode(errorCode);
			}
		}
		return NtStatus::FileClosed;
	}

	NtStatus MirrorFS::OnFindFiles(DynamicStringRefW path, DynamicStringRefW pattern, EvtFindFiles* event1, EvtFindFilesWithPattern* event2)
	{
		DynamicStringW targetPath = DispatchLocationRequest(path);
		size_t targetPathLength = targetPath.length();

		auto AppendAsterix = [pattern](DynamicStringW& path)
		{
			if (!path.empty())
			{
				if (path.back() != '\\')
				{
					path += L'\\';
				}
				path += pattern;
			}
		};
		AppendAsterix(targetPath);

		WIN32_FIND_DATAW findData = {0};
		SearchHandle findHandle = ::FindFirstFileW(targetPath, &findData);
		if (!findHandle.IsValid())
		{
			return GetNtStatusByWin32LastErrorCode();
		}

		// Root folder does not have . and .. folder - we remove them
		const bool isRootFolder = path == L"\\";

		size_t count = 0;
		do
		{
			if (!isRootFolder || (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
			{
				if (event1)
				{
					OnFileFound(*event1, findData);
				}
				else
				{
					OnFileFound(*event2, findData);
				}
			}
			count++;
		}
		while (::FindNextFileW(findHandle, &findData) != 0);

		const DWORD errorCode = GetLastError();
		if (errorCode != ERROR_NO_MORE_FILES)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return NtStatus::Success;
	}
	NtStatus MirrorFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		return OnFindFiles(eventInfo.PathName, L"*", &eventInfo, nullptr);
	}
	NtStatus MirrorFS::OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo)
	{
		return OnFindFiles(eventInfo.PathName, eventInfo.SearchPattern, nullptr, &eventInfo);
	}
	NtStatus MirrorFS::OnFindStreams(EvtFindStreams& eventInfo)
	{
		DynamicStringW targetPath = DispatchLocationRequest(eventInfo.FileName);

		WIN32_FIND_STREAM_DATA findData = {0};
		SearchHandle findHandle = ::FindFirstStreamW(targetPath, FindStreamInfoStandard, &findData, 0);
		if (!findHandle.IsValid())
		{
			return GetNtStatusByWin32LastErrorCode();
		}

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

		if (findResult == Dokany2::DOKAN_STREAM_BUFFER_FULL)
		{
			// FindStreams returned 'count' entries in 'sInSourcePath' with NtStatus::BufferOverflow
			// https://msdn.microsoft.com/en-us/library/windows/hardware/ff540364(v=vs.85).aspx
			return NtStatus::BufferOverflow;
		}

		const DWORD errorCode = GetLastError();
		if (errorCode != ERROR_HANDLE_EOF)
		{
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
		return NtStatus::Success;
	}
}
