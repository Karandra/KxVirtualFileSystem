/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Mirror/MirrorFS.h"
#include "KxVirtualFileSystem/Mirror/MirrorStructs.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ConvergenceFS.h"
#include <AclAPI.h>
#pragma warning (disable: 4267)

namespace KxVFS
{
	void ConvergenceFS::MakeFilePath(KxDynamicStringW& outPath, KxDynamicStringRefW folder, KxDynamicStringRefW file) const
	{
		outPath = Utility::LongPathPrefix;
		outPath += folder;
		outPath += file;
	}
	bool ConvergenceFS::IsPathPresent(KxDynamicStringRefW requestedPath, KxDynamicStringRefW* virtualFolder) const
	{
		KxDynamicStringW path;
		for (auto i = m_VirtualFolders.rbegin(); i != m_VirtualFolders.rend(); ++i)
		{
			MakeFilePath(path, *i, requestedPath);
			if (Utility::IsExist(path))
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
	bool ConvergenceFS::IsPathPresentInWriteTarget(KxDynamicStringRefW requestedPath) const
	{
		KxDynamicStringW path;
		MakeFilePath(path, GetWriteTarget(), requestedPath);
		return Utility::IsExist(path);
	}

	bool ConvergenceFS::IsWriteRequest(KxDynamicStringRefW filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
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
				(createDisposition == CREATE_NEW && !Utility::IsExist(filePath)) ||
				(createDisposition == OPEN_ALWAYS && !Utility::IsExist(filePath)) ||
				(createDisposition == TRUNCATE_EXISTING && Utility::IsExist(filePath))
				);
	}
	bool ConvergenceFS::IsReadRequest(KxDynamicStringRefW filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
	{
		return !IsWriteRequest(filePath, desiredAccess, createDisposition);
	}
	bool ConvergenceFS::IsDirectory(ULONG kernelCreateOptions) const
	{
		return kernelCreateOptions & FILE_DIRECTORY_FILE;
	}
	bool ConvergenceFS::IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const
	{
		return ((*securityInformation & SACL_SECURITY_INFORMATION) || (*securityInformation & BACKUP_SECURITY_INFORMATION));
	}
	void ConvergenceFS::ProcessSESecurityPrivilege(bool hasSESecurityPrivilege, PSECURITY_INFORMATION securityInformation) const
	{
		if (!hasSESecurityPrivilege)
		{
			*securityInformation &= ~SACL_SECURITY_INFORMATION;
			*securityInformation &= ~BACKUP_SECURITY_INFORMATION;
		}
	}
}

// IRequestDispatcher
namespace KxVFS
{
	void ConvergenceFS::ResolveLocation(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath)
	{
		// If request to root return path to write target
		if (IsRequestToRoot(requestedPath))
		{
			MakeFilePath(targetPath, GetWriteTarget(), requestedPath);
		}
		else
		{
			// Try to dispatch normal request
			if (TryDispatchRequest(requestedPath, targetPath))
			{
				// File found in index
				return;
			}
			else
			{
				// File tot found in index. Search it in write target and in all virtual folders and save to index if it found.
				KxDynamicStringW inWriteTarget;
				MakeFilePath(inWriteTarget, GetWriteTarget(), requestedPath);

				if (!Utility::IsExist(inWriteTarget))
				{
					for (auto i = m_VirtualFolders.rbegin(); i != m_VirtualFolders.rend(); ++i)
					{
						MakeFilePath(targetPath, *i, requestedPath);
						if (Utility::IsExist(targetPath))
						{
							UpdateDispatcherIndex(requestedPath, targetPath);
							return;
						}
					}
				}

				// Not found, probably new file, return location in write target.
				UpdateDispatcherIndex(requestedPath, inWriteTarget);
				targetPath = inWriteTarget;
			}
		}
	}
	bool ConvergenceFS::TryDispatchRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) const
	{
		auto it = m_RequestDispatcherIndex.find(NormalizePath(requestedPath));
		if (it != m_RequestDispatcherIndex.end())
		{
			targetPath = it->second;
			return true;
		}
		return false;
	}

	bool ConvergenceFS::UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath, KxDynamicStringRefW targetPath)
	{
		if (!requestedPath.empty() && requestedPath != L"\\")
		{
			const bool isInserted = m_RequestDispatcherIndex.insert_or_assign(NormalizePath(requestedPath), NormalizePath(targetPath)).second;
			return isInserted;
		}
		return false;
	}
	void ConvergenceFS::UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath)
	{
		m_RequestDispatcherIndex.erase(NormalizePath(requestedPath));
	}
}

// IEnumerationDispatcher
namespace KxVFS
{
	ConvergenceFS::TEnumerationVector* ConvergenceFS::GetEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		auto it = m_EnumerationDispatcherIndex.find(NormalizePath(requestedPath));
		if (it != m_EnumerationDispatcherIndex.end())
		{
			return &(it->second);
		}
		return nullptr;
	}
	ConvergenceFS::TEnumerationVector& ConvergenceFS::CreateEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		auto it = m_EnumerationDispatcherIndex.insert_or_assign(NormalizePath(requestedPath), TEnumerationVector());
		return it.first->second;
	}
	
	void ConvergenceFS::InvalidateEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		CriticalSectionLocker lockWrite(m_EnumerationDispatcherIndexCS);
		
		m_EnumerationDispatcherIndex.erase(NormalizePath(requestedPath));
	}
	void ConvergenceFS::InvalidateSearchDispatcherVectorForFile(KxDynamicStringRefW requestedFilePath)
	{
		size_t pos = requestedFilePath.rfind(L'\\');
		if (pos != KxDynamicStringRefW::npos)
		{
			requestedFilePath.remove_prefix(pos);
		}
		if (requestedFilePath.empty())
		{
			requestedFilePath = L"\\";
		}
		InvalidateEnumerationVector(requestedFilePath);
	}
}

// Non-existent INI-files
namespace KxVFS
{
	bool ConvergenceFS::IsINIFile(KxDynamicStringRefW requestedPath) const
	{
		return Dokany2::DokanIsNameInExpression(L"*.ini", requestedPath.data(), TRUE);
	}
	bool ConvergenceFS::IsINIFileNonExistent(KxDynamicStringRefW requestedPath) const
	{
		return m_NonExistentINIFiles.count(NormalizePath(requestedPath)) != 0;
	}
	void ConvergenceFS::AddINIFile(KxDynamicStringRefW requestedPath)
	{
		CriticalSectionLocker lockWrite(m_NonExistentINIFilesCS);

		m_NonExistentINIFiles.insert(NormalizePath(requestedPath));
	}
}

namespace KxVFS
{
	KxDynamicStringRefW& ConvergenceFS::NormalizePath(KxDynamicStringRefW& requestedPath) const
	{
		// See if path starts with '\' and remove it. Don't touch '\\?\'.
		if (!requestedPath.empty() && requestedPath.find_first_of(Utility::LongPathPrefix, 0, std::size(Utility::LongPathPrefix)) == KxDynamicStringRefW::npos)
		{
			size_t count = 0;
			for (const auto& c: requestedPath)
			{
				if (c == L'\\')
				{
					++count;
				}
				else
				{
					break;
				}
			}
			requestedPath.remove_prefix(count);
		}

		// Remove any trailing '\\'
		size_t count = 0;
		for (auto it = requestedPath.rbegin(); it != requestedPath.rend(); ++it)
		{
			if (*it == L'\\')
			{
				++count;
			}
			else
			{
				break;
			}
		}
		requestedPath.remove_suffix(count);
		return requestedPath;
	}

	ConvergenceFS::ConvergenceFS(Service* vfsService, KxDynamicStringRefW mountPoint, KxDynamicStringRefW writeTarget, uint32_t flags)
		:MirrorFS(vfsService, mountPoint, writeTarget, flags)
	{
	}
	ConvergenceFS::~ConvergenceFS()
	{
	}

	FSError ConvergenceFS::Mount()
	{
		if (!IsMounted())
		{
			// Make sure write target exist
			Utility::CreateFolderTree(GetWriteTarget());

			// Preallocate some space for indexes
			m_RequestDispatcherIndex.reserve(m_VirtualFolders.size() * 32);
			m_EnumerationDispatcherIndex.reserve(m_VirtualFolders.size() * 128);

			// Mount now
			return MirrorFS::Mount();
		}
		return DOKAN_ERROR;
	}
	bool ConvergenceFS::UnMount()
	{
		m_RequestDispatcherIndex.clear();
		m_EnumerationDispatcherIndex.clear();
		m_NonExistentINIFiles.clear();

		return MirrorFS::UnMount();
	}

	bool ConvergenceFS::SetWriteTarget(KxDynamicStringRefW writeTarget)
	{
		return SetSource(writeTarget);
	}

	bool ConvergenceFS::AddVirtualFolder(KxDynamicStringRefW path)
	{
		if (!IsMounted())
		{
			m_VirtualFolders.emplace_back(NormalizePath(path));
			return true;
		}
		return false;
	}
	bool ConvergenceFS::ClearVirtualFolders()
	{
		if (!IsMounted())
		{
			m_VirtualFolders.clear();
			m_RequestDispatcherIndex.clear();
			return true;
		}
		return false;
	}

	void ConvergenceFS::BuildDispatcherIndex()
	{
		m_RequestDispatcherIndex.clear();
		m_EnumerationDispatcherIndex.clear();

		auto ExtractRequestPath = [](KxDynamicStringRefW virtualFolder, const KxDynamicStringW& targetPath)
		{
			KxDynamicStringW requestedPath(targetPath);
			size_t eraseOffset = 0;
			if (requestedPath.length() >= virtualFolder.length() && requestedPath[virtualFolder.length()] == TEXT('\\'))
			{
				eraseOffset = 1;
			}
			requestedPath.erase(0, virtualFolder.length() + eraseOffset);

			return requestedPath;
		};

		std::function<void(const KxDynamicStringRefW virtualFolder, const KxDynamicStringW& path)> Recurse;
		Recurse = [this, &Recurse, &ExtractRequestPath](const KxDynamicStringRefW virtualFolder, const KxDynamicStringW& path)
		{
			KxFileFinder finder(path, TEXT("*"));

			KxFileItem item = finder.FindNext();
			while (item.IsOK())
			{
				if (item.IsNormalItem())
				{
					// Save to file dispatcher index
					KxDynamicStringW targetPath(L"\\\\?\\");
					targetPath += item.GetFullPath();
					UpdateDispatcherIndexUnlocked(ExtractRequestPath(virtualFolder, item.GetFullPath()), targetPath);

					if (item.IsDirectory())
					{
						// Makes game crash
						#if 0
						// Save directory list to search index
						KxDynamicStringW sRequestPath = ExtractRequestPath(virtualFolder, item.GetFullPath());
						TEnumerationVector* searchIndex = GetEnumerationVector(sRequestPath);
						if (!searchIndex)
						{
							searchIndex = CreateEnumerationVector(sRequestPath);
						}

						// Enum this folder content
						KxFileFinder tFolderFinder(item.GetFullPath(), TEXT("*"));
						KxFileItem tFolderItem = tFolderFinder.FindNext();
						while (tFolderItem.IsOK())
						{
							if (tFolderItem.IsNormalItem())
							{
								// Add only if there are no file or folder with such name
								KxDynamicStringW sNameL = tFolderItem.GetName().to_lower();
								if (std::none_of(searchIndex->begin(), searchIndex->end(), [&sNameL](const WIN32_FIND_DATAW& findData)
								{
									return sNameL == KxDynamicStringW(findData.cFileName).make_lower();
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

		Recurse(GetWriteTarget(), GetWriteTarget());
		for (auto i = m_VirtualFolders.rbegin(); i != m_VirtualFolders.rend(); ++i)
		{
			Recurse(*i, *i);
		}
	}
}

namespace KxVFS
{
	NTSTATUS ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		// Non-existent INI files optimization
		const HRESULT iniOptimizationReturnCode = STATUS_OBJECT_PATH_NOT_FOUND;

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

		if (IsUnsingAsyncIO())
		{
			fileAttributesAndFlags |= FILE_FLAG_OVERLAPPED;
		}

		// This is for Impersonate Caller User Option
		TokenHandle userTokenHandle = ImpersonateCallerUserIfNeeded(eventInfo);

		// Security
		SECURITY_ATTRIBUTES securityAttributes = {0};
		Utility::SecurityObject newFileSecurity = CreateSecurityIfNeeded(eventInfo, securityAttributes, targetPath, creationDisposition);

		if (eventInfo.DokanFileInfo->IsDirectory)
		{
			/* It is a create directory request */

			if (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS)
			{
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// We create folder
				DWORD createFolderErrorCode = STATUS_SUCCESS;
				if (!Utility::CreateFolderTree(targetPath, false, &securityAttributes, &createFolderErrorCode))
				{
					//errorCode = GetLastError();
					errorCode = createFolderErrorCode;

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
				CleanupImpersonateCallerUserIfNeeded(userTokenHandle, statusCode);
			}

			if (statusCode == STATUS_SUCCESS)
			{
				// Check first if we're trying to open a file as a directory.
				if (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
				{
					return STATUS_NOT_A_DIRECTORY;
				}
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
				FileHandle fileHandle = CreateFileW(targetPath,
													genericDesiredAccess,
													eventInfo.ShareAccess,
													&securityAttributes,
													OPEN_EXISTING,
													fileAttributesAndFlags|FILE_FLAG_BACKUP_SEMANTICS,
													nullptr
				);
				CleanupImpersonateCallerUserIfNeeded(userTokenHandle);

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
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// Non-existent INI files optimization
				if ((creationDisposition == OPEN_ALWAYS || creationDisposition == OPEN_EXISTING) && IsINIFile(eventInfo.FileName))
				{
					// If file doesn't exist
					bool isNotExist = IsINIFileNonExistent(eventInfo.FileName);
					if (isNotExist || !Utility::IsFileExist(targetPath))
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
					Utility::CreateFolderTree(targetPath, true);
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
					if (fileAttributes != INVALID_FILE_ATTRIBUTES && creationDisposition == TRUNCATE_EXISTING)
					{
						SetFileAttributesW(targetPath, fileAttributesAndFlags|fileAttributes);
					}

					// Invalidate folder content if file is created or overwritten
					if (creationDisposition == CREATE_NEW || creationDisposition == CREATE_ALWAYS || creationDisposition == OPEN_ALWAYS || creationDisposition == TRUNCATE_EXISTING)
					{
						InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);
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
						eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(mirrorContext);

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
	NTSTATUS ConvergenceFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		Mirror::FileContext* mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
		if (mirrorContext)
		{
			KxDynamicStringW targetPathOld;
			ResolveLocation(eventInfo.FileName, targetPathOld);

			KxDynamicStringW targetPathNew;
			ResolveLocation(eventInfo.NewFileName, targetPathNew);

			Utility::CreateFolderTree(targetPathNew, true);
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

	NTSTATUS ConvergenceFS::OnReadFile(EvtReadFile& eventInfo)
	{
		Mirror::FileContext *mirrorContext = (Mirror::FileContext*)eventInfo.DokanFileInfo->Context;
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
			KxVFSDebugPrint(L"File is cleaned up, reopening: %s", targetPath.data());

			FileHandle tempFileHandle = CreateFileW(targetPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (tempFileHandle.IsOK())
			{
				return ReadFileSynchronous(eventInfo, tempFileHandle);
			}
			return GetNtStatusByWin32LastErrorCode();
		}

		if (IsUnsingAsyncIO())
		{
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
		}
		else
		{
			KxVFSDebugPrint(L"Reading file by handle: %s", eventInfo.FileName);
			return ReadFileSynchronous(eventInfo, mirrorContext->m_FileHandle);
		}
	}
	NTSTATUS ConvergenceFS::OnWriteFile(EvtWriteFile& eventInfo)
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

		// This write most likely change file attributes
		InvalidateSearchDispatcherVectorForFile(eventInfo.FileName);

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

		if (IsUnsingAsyncIO())
		{
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
		}
		else
		{
			return WriteFileSynchronous(eventInfo, mirrorContext->m_FileHandle, fileSize);
		}
	}

	DWORD ConvergenceFS::OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, Utility::StringSearcherHash& hashStore, TEnumerationVector* searchIndex)
	{
		DWORD errorCode = ERROR_NO_MORE_FILES;

		WIN32_FIND_DATAW findData = {0};
		HANDLE findHandle = FindFirstFileW(path, &findData);
		if (findHandle != INVALID_HANDLE_VALUE)
		{
			KxDynamicStringW fileName;
			do
			{
				// Hash only lowercase version of name
				fileName.assign(findData.cFileName);
				fileName.make_lower();

				// If this file is not found already
				size_t hashValue = Utility::HashString(fileName);
				if (hashStore.emplace(hashValue).second)
				{
					eventInfo.FillFindData(&eventInfo, &findData);
					searchIndex->push_back(findData);

					// Save this path into dispatcher index.
					KxDynamicStringW requestedPath(eventInfo.PathName);
					if (!requestedPath.empty() && requestedPath.back() != TEXT('\\'))
					{
						requestedPath += TEXT('\\');
					}
					requestedPath += findData.cFileName;

					// Remove '*' at end.
					KxDynamicStringW targetPath(path);
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
	NTSTATUS ConvergenceFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		CriticalSectionLocker lockIndex(m_EnumerationDispatcherIndexCS);

		TEnumerationVector* searchIndex = GetEnumerationVector(eventInfo.PathName);
		if (searchIndex)
		{
			SendEnumerationVector(&eventInfo, *searchIndex);
			return STATUS_SUCCESS;
		}
		else
		{
			searchIndex = &CreateEnumerationVector(eventInfo.PathName);
		}

		//////////////////////////////////////////////////////////////////////////
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

		DWORD errorCode = 0;
		Utility::StringSearcherHash foundPaths = {Utility::HashString(L"."), Utility::HashString(L"..")};

		// Find everything in write target first as it have highest priority
		KxDynamicStringW writeTarget;
		MakeFilePath(writeTarget, GetWriteTarget(), eventInfo.PathName);
		AppendAsterix(writeTarget);

		errorCode = OnFindFilesAux(writeTarget, eventInfo, foundPaths, searchIndex);

		// Then in other folders
		for (auto i = m_VirtualFolders.rbegin(); i != m_VirtualFolders.rend(); ++i)
		{
			KxDynamicStringW path;
			MakeFilePath(path, *i, eventInfo.PathName);
			AppendAsterix(path);

			errorCode = OnFindFilesAux(path, eventInfo, foundPaths, searchIndex);
		}

		if (errorCode != ERROR_NO_MORE_FILES)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
}
