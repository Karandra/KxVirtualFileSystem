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

// IRequestDispatcher
namespace KxVFS
{
	void ConvergenceFS::MakeFilePath(KxDynamicStringW& outPath, KxDynamicStringRefW folder, KxDynamicStringRefW file) const
	{
		outPath = Utility::LongPathPrefix;
		outPath += folder;
		outPath += file;
	}
	bool ConvergenceFS::TryDispatchRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) const
	{
		auto it = m_RequestDispatcherIndex.find(NormalizeFilePath(requestedPath));
		if (it != m_RequestDispatcherIndex.end())
		{
			targetPath = it->second;
			return true;
		}
		return false;
	}
	void ConvergenceFS::DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath)
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
					for (auto it = m_VirtualFolders.rbegin(); it != m_VirtualFolders.rend(); ++it)
					{
						MakeFilePath(targetPath, *it, requestedPath);
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

	bool ConvergenceFS::UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath, KxDynamicStringRefW targetPath)
	{
		if (!requestedPath.empty() && requestedPath != L"\\")
		{
			const bool isInserted = m_RequestDispatcherIndex.insert_or_assign(NormalizeFilePath(requestedPath), NormalizeFilePath(targetPath)).second;
			return isInserted;
		}
		return false;
	}
	void ConvergenceFS::UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath)
	{
		m_RequestDispatcherIndex.erase(NormalizeFilePath(requestedPath));
	}
	bool ConvergenceFS::UpdateDispatcherIndex(KxDynamicStringRefW requestedPath, KxDynamicStringRefW targetPath)
	{
		if (CriticalSectionLocker lock(m_RequestDispatcherIndexCS); true)
		{
			return UpdateDispatcherIndexUnlocked(requestedPath, targetPath);
		}
	}
	void ConvergenceFS::UpdateDispatcherIndex(KxDynamicStringRefW requestedPath)
	{
		if (CriticalSectionLocker lock(m_RequestDispatcherIndexCS); true)
		{
			UpdateDispatcherIndexUnlocked(requestedPath);
		}
	}
}

// IEnumerationDispatcher
namespace KxVFS
{
	ConvergenceFS::TEnumerationVector* ConvergenceFS::GetEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		auto it = m_EnumerationDispatcherIndex.find(NormalizeFilePath(requestedPath));
		if (it != m_EnumerationDispatcherIndex.end())
		{
			return &(it->second);
		}
		return nullptr;
	}
	ConvergenceFS::TEnumerationVector& ConvergenceFS::CreateEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		auto it = m_EnumerationDispatcherIndex.insert_or_assign(NormalizeFilePath(requestedPath), TEnumerationVector());
		return it.first->second;
	}
	
	void ConvergenceFS::InvalidateEnumerationVector(KxDynamicStringRefW requestedPath)
	{
		if (CriticalSectionLocker lock(m_EnumerationDispatcherIndexCS); true)
		{
			m_EnumerationDispatcherIndex.erase(NormalizeFilePath(requestedPath));
		}
	}
	void ConvergenceFS::InvalidateSearchDispatcherVectorUsingFile(KxDynamicStringRefW requestedFilePath)
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
		return m_NonExistentINIFiles.count(NormalizeFilePath(requestedPath)) != 0;
	}
	void ConvergenceFS::AddINIFile(KxDynamicStringRefW requestedPath)
	{
		if (CriticalSectionLocker lock(m_NonExistentINIFilesCS); true)
		{
			m_NonExistentINIFiles.insert(NormalizeFilePath(requestedPath));
		}
	}
	void ConvergenceFS::RemoveINIFile(KxDynamicStringRefW requestedPath)
	{
		if (CriticalSectionLocker lock(m_NonExistentINIFilesCS); true)
		{
			m_NonExistentINIFiles.erase(NormalizeFilePath(requestedPath));
		}
	}
}

namespace KxVFS
{
	ConvergenceFS::ConvergenceFS(Service& service, KxDynamicStringRefW mountPoint, KxDynamicStringRefW writeTarget, uint32_t flags)
		:MirrorFS(service, mountPoint, writeTarget, flags)
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
}

namespace KxVFS
{
	KxDynamicStringRefW ConvergenceFS::GetWriteTarget() const
	{
		return GetSource();
	}
	void ConvergenceFS::SetWriteTarget(KxDynamicStringRefW writeTarget)
	{
		SetSource(writeTarget);
	}

	void ConvergenceFS::AddVirtualFolder(KxDynamicStringRefW path)
	{
		if (!IsMounted())
		{
			m_VirtualFolders.emplace_back(NormalizeFilePath(path));
		}
	}
	void ConvergenceFS::ClearVirtualFolders()
	{
		if (!IsMounted())
		{
			m_VirtualFolders.clear();
			m_RequestDispatcherIndex.clear();
		}
	}

	size_t ConvergenceFS::BuildDispatcherIndex()
	{
		m_RequestDispatcherIndex.clear();
		m_EnumerationDispatcherIndex.clear();

		Utility::SimpleDispatcherMapBuilder builder([this](KxDynamicStringRefW virtualFolder, KxDynamicStringRefW requestPath, KxDynamicStringRefW targetPath)
		{
			UpdateDispatcherIndexUnlocked(requestPath, targetPath);
		});

		builder.Run(GetWriteTarget());
		for (auto it = m_VirtualFolders.rbegin(); it != m_VirtualFolders.rend(); ++it)
		{
			builder.Run(*it);
		}

		return m_RequestDispatcherIndex.size();
	}

	bool ConvergenceFS::IsINIOptimizationEnabled() const
	{
		return m_IsINIOptimizationEnabled;
	}
	void ConvergenceFS::EnableINIOptimization(bool value)
	{
		SetOptionIfNotMounted(m_IsINIOptimizationEnabled, value);
	}
}

namespace KxVFS
{
	NTSTATUS ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		DWORD errorCode = 0;
		NTSTATUS statusCode = STATUS_SUCCESS;

		KxDynamicStringW targetPath;
		DispatchLocationRequest(eventInfo.FileName, targetPath);

		auto[fileAttributesAndFlags, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);

		// When filePath is a directory, needs to change the flag so that the file can be opened.
		const FileAttributes fileAttributes = FromInt<FileAttributes>(::GetFileAttributesW(targetPath));
		if (fileAttributes != FileAttributes::Invalid)
		{
			if (ToBool(fileAttributes & FileAttributes::Directory))
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
			/* It is a create directory request */

			if (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::OpenAlways)
			{
				ImpersonateLoggedOnUserIfNeeded(userTokenHandle);

				// We create folder
				DWORD createFolderErrorCode = STATUS_SUCCESS;
				if (!Utility::CreateFolderTree(targetPath, false, &securityAttributes, &createFolderErrorCode))
				{
					//errorCode = GetLastError();
					errorCode = createFolderErrorCode;

					// Fail to create folder for OPEN_ALWAYS is not an error
					if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CreationDisposition::CreateNew)
					{
						statusCode = GetNtStatusByWin32ErrorCode(errorCode);
					}
				}
				else
				{
					// Invalidate containing folder content
					InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
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
				Utility::FileHandle fileHandle = ::CreateFileW(targetPath,
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
			/* It is a create file request */

			if (!CheckAttributesToOverwriteFile(fileAttributes, fileAttributesAndFlags, creationDisposition))
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

				// If we are asked to create a file, try to create its folder first
				const bool isWriteRequest = IsWriteRequest(targetPath, genericDesiredAccess, creationDisposition);
				if (isWriteRequest)
				{
					Utility::CreateFolderTree(targetPath, true);
				}

				// Non-existent INI files optimization
				if (m_IsINIOptimizationEnabled && IsINIFile(eventInfo.FileName))
				{
					const bool existOnDisk = Utility::IsFileExist(targetPath);
					const bool knownAsInvalid = IsINIFileNonExistent(eventInfo.FileName);

					// Remove that on write, the INI might be actually written.
					if (isWriteRequest)
					{
						RemoveINIFile(eventInfo.FileName);
					}
					else if (!existOnDisk)
					{
						AddINIFile(eventInfo.FileName);
						return STATUS_OBJECT_PATH_NOT_FOUND;
					}
				}

				KxVFSDebugPrint(L"Trying to create/open file: %s", targetPath.data());

				OpenWithSecurityAccessIfNeeded(genericDesiredAccess, isWriteRequest);
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

					// Invalidate folder content if file is created or overwritten
					if (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::CreateAlways || creationDisposition == CreationDisposition::OpenAlways || creationDisposition == CreationDisposition::TruncateExisting)
					{
						InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
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
	NTSTATUS ConvergenceFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		if (Mirror::FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxDynamicStringW targetPathOld;
			DispatchLocationRequest(eventInfo.FileName, targetPathOld);

			KxDynamicStringW targetPathNew;
			DispatchLocationRequest(eventInfo.NewFileName, targetPathNew);

			Utility::CreateFolderTree(targetPathNew, true);
			bool isOK = ::MoveFileExW(targetPathOld, targetPathNew, MOVEFILE_COPY_ALLOWED|(eventInfo.ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0));
			if (isOK)
			{
				UpdateDispatcherIndex(eventInfo.FileName);
				UpdateDispatcherIndex(eventInfo.NewFileName);

				InvalidateSearchDispatcherVectorUsingFile(eventInfo.FileName);
				InvalidateSearchDispatcherVectorUsingFile(eventInfo.NewFileName);

				return STATUS_SUCCESS;
			}
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_FILE_CLOSED;
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
		if (CriticalSectionLocker lockIndex(m_EnumerationDispatcherIndexCS); true)
		{
			TEnumerationVector* searchIndex = GetEnumerationVector(eventInfo.PathName);
			if (searchIndex && !searchIndex->empty())
			{
				SendEnumerationVector(&eventInfo, *searchIndex);
				return STATUS_SUCCESS;
			}
			if (!searchIndex)
			{
				searchIndex = &CreateEnumerationVector(eventInfo.PathName);
			}

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

			Utility::StringSearcherHash foundPaths = {Utility::HashString(L"."), Utility::HashString(L"..")};

			// Find everything in write target first as it have highest priority
			KxDynamicStringW writeTarget;
			MakeFilePath(writeTarget, GetWriteTarget(), eventInfo.PathName);
			AppendAsterix(writeTarget);

			DWORD errorCode = OnFindFilesAux(writeTarget, eventInfo, foundPaths, searchIndex);

			// Then in other folders
			for (auto it = m_VirtualFolders.rbegin(); it != m_VirtualFolders.rend(); ++it)
			{
				KxDynamicStringW path;
				MakeFilePath(path, *it, eventInfo.PathName);
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
}
