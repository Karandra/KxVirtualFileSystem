/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ConvergenceFS.h"

namespace KxVFS
{
	KxDynamicStringW ConvergenceFS::MakeFilePath(KxDynamicStringRefW baseDirectory, KxDynamicStringRefW requestedPath, bool addNamespace) const
	{
		KxDynamicStringW outPath = addNamespace ? Utility::GetLongPathPrefix() : KxNullDynamicStringW;
		outPath += baseDirectory;
		outPath += requestedPath;

		return outPath;
	}
	std::tuple<KxDynamicStringW, KxDynamicStringRefW> ConvergenceFS::GetTargetPath(const FileNode* node, KxDynamicStringRefW requestedPath, bool addNamespace) const
	{
		if (node && !node->IsRootNode())
		{
			// File found in index
			return {addNamespace ? node->GetFullPathWithNS() : node->GetFullPath(), node->GetVirtualDirectory()};
		}
		else
		{
			// Not found, probably new file, return location in write target.
			return {MakeFilePath(GetWriteTarget(), requestedPath, addNamespace), GetWriteTarget()};
		}
	}
	KxDynamicStringW ConvergenceFS::DispatchLocationRequest(KxDynamicStringRefW requestedPath)
	{
		return std::get<0>(GetTargetPath(m_VirtualTree.NavigateToAny(requestedPath), requestedPath, true));
	}

	bool ConvergenceFS::ProcessDeleteOnClose(Dokany2::DOKAN_FILE_INFO& fileInfo, FileNode& fileNode) const
	{
		if (fileInfo.DeleteOnClose)
		{
			// Should already be deleted by 'CloseHandle' if opened with 'FILE_FLAG_DELETE_ON_CLOSE'.
			if (auto parentLock = fileNode.GetParent()->LockExclusive(); true)
			{
				bool success = false;
				KxDynamicStringW fullPath = fileNode.GetFullPathWithNS();
				if (fileNode.IsDirectory())
				{
					success = ::RemoveDirectoryW(fullPath);
				}
				else
				{
					success = ::DeleteFileW(fullPath);
				}

				if (success)
				{
					fileNode.RemoveThisChild();
				}
				return success;
			}
		}
		return false;
	}
	bool ConvergenceFS::IsWriteTargetNode(const FileNode& fileNode) const
	{
		return fileNode.GetVirtualDirectory() == GetWriteTarget();
	}
}

namespace KxVFS
{
	ConvergenceFS::ConvergenceFS(FileSystemService& service, KxDynamicStringRefW mountPoint, KxDynamicStringRefW writeTarget, FSFlags flags)
		:MirrorFS(service, mountPoint, writeTarget, flags)
	{
	}

	FSError ConvergenceFS::Mount()
	{
		if (!IsMounted())
		{
			// Make sure write target exist
			Utility::CreateDirectoryTree(GetWriteTarget());

			// Build virtual tree if it wasn't built before
			if (!m_VirtualTree.HasChildren())
			{
				BuildFileTree();
			}

			// Mount now
			return MirrorFS::Mount();
		}
		return FSErrorCode::CanNotMount;
	}
	bool ConvergenceFS::UnMount()
	{
		m_VirtualTree.MakeNull();
		return MirrorFS::UnMount();
	}

	void ConvergenceFS::AddVirtualFolder(KxDynamicStringRefW path)
	{
		m_VirtualFolders.emplace_back(Utility::NormalizeFilePath(path));
	}
	void ConvergenceFS::ClearVirtualFolders()
	{
		m_VirtualFolders.clear();
	}
	size_t ConvergenceFS::BuildFileTree()
	{
		m_VirtualTree.MakeNull();
		m_VirtualTree.UpdateItemInfo(GetMountPoint());

		// We are going to push write target path later, so preallocate space for it.
		// It's need for references validity below.
		m_VirtualFolders.reserve(m_VirtualFolders.capacity() + 1);

		// Create individual virtual trees
		Utility::Comparator::UnorderedMapNoCase<std::unique_ptr<FileNode>> virtualNodes;
		for (const KxDynamicStringW& path: m_VirtualFolders)
		{
			// Paths references here belong to 'm_VirtualFolders' vector
			auto[it, _] = virtualNodes.emplace(path, std::make_unique<FileNode>(path.get_view()));
			it->second->UpdateFileTree(path);
		}
		{
			// Base class owns result of 'GetWriteTarget()' so all references are valid
			auto[it, _] = virtualNodes.insert_or_assign(GetWriteTarget(), std::make_unique<FileNode>(GetWriteTarget()));
			it->second->UpdateFileTree(GetWriteTarget());
		}

		auto BuildTreeBranch = [this, &virtualNodes](FileNode& rootNode, FileNode::RefVector& directories)
		{
			Utility::Comparator::UnorderedSetNoCase hash;
			hash.reserve(m_VirtualFolders.size());
			const KxDynamicStringW rootPath = rootNode.GetRelativePath();

			for (auto it = m_VirtualFolders.rbegin(); it != m_VirtualFolders.rend(); ++it)
			{
				auto nodeIt = virtualNodes.find(*it);
				const KxDynamicStringW& folderPath = nodeIt->first;
				FileNode& folderNode = *nodeIt->second;

				// If we have root node, look for files in real file tree, otherwise use mod's tree root
				if (FileNode* searchNode = folderNode.NavigateToFolder(rootPath))
				{
					// Most likely not enough, but at least something
					rootNode.ReserveChildren(searchNode->GetChildrenCount());

					for (const auto& [name, node]: searchNode->GetChildren())
					{
						auto hashIt = hash.insert(name);
						if (hashIt.second)
						{
							FileNode& newNode = rootNode.AddChild(std::make_unique<FileNode>(node->GetItem(), &rootNode));
							newNode.CopyBasicAttributes(*node);

							if (newNode.IsDirectory())
							{
								directories.push_back(&newNode);
							}
						}
					}
				}
			}
		};

		// Temporarily add write target to build tree with it and remove when we are done.
		m_VirtualFolders.push_back(GetWriteTarget());
		KxCallAtScopeExit atExit([this]()
		{
			m_VirtualFolders.pop_back();
		});

		// Build top level
		FileNode::RefVector directories;
		BuildTreeBranch(m_VirtualTree, directories);

		// Build subdirectories
		while (!directories.empty())
		{
			FileNode::RefVector roundDirectories;
			roundDirectories.reserve(directories.size());

			for (FileNode* node: directories)
			{
				BuildTreeBranch(*node, roundDirectories);
			}
			directories = std::move(roundDirectories);
		}

		// Count all nodes count for diagnostic purposes
		size_t totalCount = 0;
		m_VirtualTree.WalkTree([&totalCount](const FileNode& node)
		{
			totalCount += node.GetChildrenCount() + 1;
			return true;
		});
		return totalCount;
	}
}

namespace KxVFS
{
	NTSTATUS ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		// Paths and nodes
		FileNode* parentNode = nullptr;
		FileNode* targetNode = nullptr;
		if (auto lock = m_VirtualTree.LockShared(); true)
		{
			targetNode = m_VirtualTree.NavigateToAny(eventInfo.FileName, parentNode);
		}

		// Attributes and flags
		const KernelFileOptions kernelOptions = FromInt<KernelFileOptions>(eventInfo.CreateOptions);

		// Correct flags
		if (targetNode)
		{
			// When the item is a directory, we need to change the flag so that the file can be opened
			if (targetNode->IsDirectory())
			{
				if (!(kernelOptions & KernelFileOptions::NonDirectoryFile))
				{
					// Needed by FindFirstFile to list files in it
					// TODO: use ReOpenFile in 'OnFindFiles' to set share read temporary
					eventInfo.DokanFileInfo->IsDirectory = TRUE;
					eventInfo.ShareAccess |= FILE_SHARE_READ;
				}
				else
				{
					// Can't open a directory as a file
					return STATUS_FILE_IS_A_DIRECTORY;
				}
			}

			// Sometimes 'DokanFileInfo.IsDirectory' is set to TRUE but 'DokanFileInfo::FileName' points to a file.
			// I'm not sure if I really need to handle this case. Everything seems to be working without this correction.
			#if 0
			if (targetNode->IsFile() && eventInfo.DokanFileInfo->IsDirectory)
			{
				KxVFS_DebugPrint(L"Object \"%s\" is file, but opening as directory is requested. Changing request parameter.", targetNode->GetFullPathWithNS().data());
				eventInfo.DokanFileInfo->IsDirectory = FALSE;
			}
			#endif
		}
		else if (kernelOptions & KernelFileOptions::DirectoryFile)
		{
			eventInfo.DokanFileInfo->IsDirectory = TRUE;
			eventInfo.ShareAccess |= FILE_SHARE_READ;
		}

		// Dispatch call
		if (eventInfo.DokanFileInfo->IsDirectory)
		{
			return OnCreateDirectory(eventInfo, targetNode, parentNode);
		}
		else
		{
			return OnCreateFile(eventInfo, targetNode, parentNode);
		}
	}
	NTSTATUS ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode)
	{
		KxVFS_DebugPrint(L"Trying to create/open file: %s", eventInfo.FileName);

		// Flags and attributes
		const FileAttributes fileAttributes = targetNode ? targetNode->GetAttributes() : FileAttributes::Invalid;
		const FileShare fileShareOptions = FromInt<FileShare>(eventInfo.ShareAccess);
		auto[requestAttributes, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);
		
		const IOManager& ioManager = GetIOManager();
		if (ioManager.IsAsyncIOEnabled())
		{
			requestAttributes |= FileAttributes::FlagOverlapped;
		}

		// Can't open non-existing file
		if (!targetNode && (creationDisposition == CreationDisposition::OpenExisting || creationDisposition == CreationDisposition::TruncateExisting))
		{
			return STATUS_OBJECT_PATH_NOT_FOUND;
		}

		// Cannot overwrite a hidden or system file if flag not set
		if (!CheckAttributesToOverwriteFile(fileAttributes, requestAttributes, creationDisposition))
		{
			return STATUS_ACCESS_DENIED;
		}

		// Truncate should always be used with write access
		if (creationDisposition == CreationDisposition::TruncateExisting)
		{
			genericDesiredAccess |= AccessRights::GenericWrite;
		}

		// Now test if there are possibility to create file
		const bool isWriteRequest = IsWriteRequest(targetNode, genericDesiredAccess, creationDisposition);
		auto[targetPath, virtualDirectory] = GetTargetPath(targetNode, eventInfo.FileName, true);

		// This is for Impersonate Caller User Option
		TokenHandle userTokenHandle = ImpersonateCallerUserIfEnabled(eventInfo);
		SecurityObject newFileSecurity = CreateSecurityIfEnabled(eventInfo, targetPath, creationDisposition);

		// Create or open file
		ImpersonateLoggedOnUserIfEnabled(userTokenHandle);
		OpenWithSecurityAccessIfEnabled(genericDesiredAccess, isWriteRequest);
		
		auto OpenOrCreateFile = [&]()
		{
			return FileHandle(targetPath, genericDesiredAccess, fileShareOptions, creationDisposition, requestAttributes, &newFileSecurity.GetAttributes());
		};
		
		FileHandle fileHandle = OpenOrCreateFile();
		if (isWriteRequest && !fileHandle && ::GetLastError() == ERROR_PATH_NOT_FOUND)
		{
			// That probably means that we're trying to create a file in write target, but there's
			// no corresponding directory tree there. So create the directory three and try again.
			KxVFS_DebugPrint(L"Attempt to create a file in non-existent directory tree in write target: %s", eventInfo.FileName);

			::SetLastError(ERROR_SUCCESS);
			KxDynamicStringW folderPath = KxDynamicStringW(eventInfo.FileName).before_last(L'\\');
			KxVFS_DebugPrint(L"Creating directory tree in write target: %s", folderPath.data());

			Utility::CreateDirectoryTreeEx(virtualDirectory, folderPath);
			fileHandle = OpenOrCreateFile();
		}

		const DWORD errorCode = ::GetLastError();
		CleanupImpersonateCallerUserIfEnabled(userTokenHandle);

		if (fileHandle)
		{
			// Add file to the virtual tree if it's new file
			if (!targetNode)
			{
				if (!parentNode)
				{
					::SetLastError(ERROR_INTERNAL_ERROR);
					return STATUS_INTERNAL_ERROR;
				}

				auto lock = parentNode->LockExclusive();
				targetNode = &parentNode->AddChild(std::make_unique<FileNode>(targetPath.get_view(), parentNode), virtualDirectory);
			}

			// Need to update FileAttributes with previous when overwriting file
			if (creationDisposition == CreationDisposition::TruncateExisting)
			{
				auto targetLock = targetNode->LockExclusive();

				// Update virtual tree info
				targetNode->SetAttributes(requestAttributes|fileAttributes);

				// Update file on disk
				::SetFileAttributesW(targetPath, ToInt(requestAttributes|fileAttributes));
			}

			FileContextManager& fileContextManager = GetFileContextManager();
			FileContext* fileContext = SaveFileContext(eventInfo, fileContextManager.PopContext(std::move(fileHandle)));
			if (fileContext)
			{
				// Save the file context
				fileContext->AssignFileNode(*targetNode);
				fileContext->GetEventInfo().Assign(eventInfo);
				OnFileCreated(eventInfo, *fileContext);

				if (creationDisposition == CreationDisposition::OpenAlways || creationDisposition == CreationDisposition::CreateAlways)
				{
					if (errorCode == ERROR_ALREADY_EXISTS)
					{
						// Open succeed but we need to inform the driver,
						// that the file open and not created by returning STATUS_OBJECT_NAME_COLLISION
						return STATUS_OBJECT_NAME_COLLISION;
					}
				}
			}
			else
			{
				::SetLastError(ERROR_INTERNAL_ERROR);
				return STATUS_INTERNAL_ERROR;
			}
			return STATUS_SUCCESS;
		}
		else
		{
			KxVFS_DebugPrint(L"Failed to create/open file: %s", targetPath.data());
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}
	NTSTATUS ConvergenceFS::OnCreateDirectory(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode)
	{
		KxVFS_DebugPrint(L"Trying to create/open directory: %s", eventInfo.FileName);

		// Flags and attributes
		const FileShare fileShareOptions = FromInt<FileShare>(eventInfo.ShareAccess);
		auto[requestAttributes, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);
		
		const IOManager& ioManager = GetIOManager();
		if (ioManager.IsAsyncIOEnabled())
		{
			requestAttributes |= FileAttributes::FlagOverlapped;
		}

		// Can't open non-existing directory
		if (!targetNode && (creationDisposition == CreationDisposition::OpenExisting || creationDisposition == CreationDisposition::TruncateExisting))
		{
			return STATUS_OBJECT_PATH_NOT_FOUND;
		}

		// Get directory path
		auto[targetPath, virtualDirectory] = GetTargetPath(targetNode, eventInfo.FileName, true);

		SecurityObject newDirectorySecurity = CreateSecurityIfEnabled(eventInfo, targetPath, creationDisposition);
		TokenHandle userTokenHandle = ImpersonateCallerUserIfEnabled(eventInfo);

		// Create folder if it's not exist
		if (!targetNode && (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::OpenAlways))
		{
			ImpersonateLoggedOnUserIfEnabled(userTokenHandle);

			NTSTATUS statusCode = STATUS_SUCCESS;
			KxCallAtScopeExit atExit([this, &userTokenHandle, &statusCode]()
			{
				CleanupImpersonateCallerUserIfEnabled(userTokenHandle, statusCode);
			});

			if (Utility::CreateDirectoryTreeEx(virtualDirectory, eventInfo.FileName, &newDirectorySecurity.GetAttributes()))
			{
				if (parentNode == nullptr)
				{
					::SetLastError(ERROR_INTERNAL_ERROR);
					return STATUS_INTERNAL_ERROR;
				}

				auto lock = parentNode->LockExclusive();
				targetNode = &parentNode->AddChild(std::make_unique<FileNode>(targetPath.get_view(), parentNode), virtualDirectory);
			}
			else
			{
				// Fail to create folder for OPEN_ALWAYS is not an error
				const DWORD errorCode = ::GetLastError();
				if (errorCode != ERROR_ALREADY_EXISTS || creationDisposition == CreationDisposition::CreateNew)
				{
					statusCode = GetNtStatusByWin32ErrorCode(errorCode);
					if (statusCode != STATUS_SUCCESS)
					{
						return statusCode;
					}
				}
			}
		}

		// Check first if we're trying to open a file as a directory.
		if (targetNode && !targetNode->IsDirectory() && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
		{
			KxVFS_DebugPrint(L"Attempt to open a file as a directory: %s", targetPath.data());
			return STATUS_NOT_A_DIRECTORY;
		}

		// FileAttributes::FlagBackupSemantics is required for opening directory handles
		ImpersonateLoggedOnUserIfEnabled(userTokenHandle);
		FileHandle directoryHandle(targetPath,
								   genericDesiredAccess,
								   fileShareOptions,
								   CreationDisposition::OpenExisting,
								   requestAttributes|FileAttributes::FlagBackupSemantics
		);
		const DWORD errorCode = ::GetLastError();
		CleanupImpersonateCallerUserIfEnabled(userTokenHandle);

		if (directoryHandle.IsValid())
		{
			// Directory node should absolutely exist at this point
			if (!targetNode)
			{
				::SetLastError(ERROR_INTERNAL_ERROR);
				return STATUS_INTERNAL_ERROR;
			}

			FileContextManager& fileContextManager = GetFileContextManager();
			FileContext* fileContext = SaveFileContext(eventInfo, fileContextManager.PopContext(std::move(directoryHandle)));
			if (fileContext)
			{
				fileContext->AssignFileNode(*targetNode);
				fileContext->GetEventInfo().Assign(eventInfo);
				OnFileCreated(eventInfo, *fileContext);
			}
			else
			{
				::SetLastError(ERROR_INTERNAL_ERROR);
				return STATUS_INTERNAL_ERROR;
			}

			// Open succeed but we need to inform the driver that the directory was open and not created
			// by returning STATUS_OBJECT_NAME_COLLISION
			if (targetNode && creationDisposition == CreationDisposition::OpenAlways)
			{
				return STATUS_OBJECT_NAME_COLLISION;
			}
			return STATUS_SUCCESS;
		}
		else
		{
			KxVFS_DebugPrint(L"Failed to create/open directory: %s", targetPath.data());
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}

	NTSTATUS ConvergenceFS::OnCanDeleteFile(EvtCanDeleteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();

				if (fileNode->IsReadOnly())
				{
					return STATUS_CANNOT_DELETE;
				}
				if (fileNode->IsDirectory() && fileNode->HasChildren())
				{
					return STATUS_DIRECTORY_NOT_EMPTY;
				}
				return STATUS_SUCCESS;
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnCloseFile(EvtCloseFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				if (auto contextLock = fileContext->Lock(); true)
				{
					fileContext->MarkClosed();
					if (fileContext->GetHandle().IsValidNonNull())
					{
						OnFileClosed(eventInfo, *fileContext);
						fileContext->CloseHandle();

						if (ProcessDeleteOnClose(*eventInfo.DokanFileInfo, *fileNode))
						{
							fileContext->ResetFileNode();
							OnFileDeleted(eventInfo, *fileContext);
						}
					}
				}

				ResetFileContext(eventInfo);
				GetFileContextManager().PushContext(*fileContext);
				return STATUS_SUCCESS;
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnCleanUp(EvtCleanUp& eventInfo)
	{
		// Look for comment in 'MirrorFS::OnCleanUp' for some important details
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				if (auto contextLock = fileContext->Lock(); true)
				{
					fileContext->CloseHandle();
					fileContext->MarkCleanedUp();
					OnFileCleanedUp(eventInfo, *fileContext);

					if (ProcessDeleteOnClose(*eventInfo.DokanFileInfo, *fileNode))
					{
						fileContext->ResetFileNode();
						OnFileDeleted(eventInfo, *fileContext);
					}
				}
				return STATUS_SUCCESS;
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}

	NTSTATUS ConvergenceFS::OnReadFile(EvtReadFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			auto[isClosed, isCleanedUp] = fileContext->InterlockedGetState();
			if (!isClosed)
			{
				IOManager& ioManager = GetIOManager();
				if (isCleanedUp)
				{
					if (FileNode* fileNode = fileContext->GetFileNode())
					{
						auto lock = fileNode->LockShared();

						FileHandle tempHandle(fileNode->GetFullPathWithNS(), AccessRights::GenericRead, FileShare::All, CreationDisposition::OpenExisting);
						if (tempHandle.IsValid())
						{
							return ioManager.ReadFileSync(tempHandle, eventInfo, fileContext);
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					return STATUS_FILE_INVALID;
				}
				else
				{
					if (ioManager.IsAsyncIOEnabled())
					{
						return ioManager.ReadFileAsync(*fileContext, eventInfo);
					}
					else
					{
						return ioManager.ReadFileSync(fileContext->GetHandle(), eventInfo, fileContext);
					}
				}
			}
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnWriteFile(EvtWriteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			auto[isClosed, isCleanedUp] = fileContext->InterlockedGetState();
			if (!isClosed)
			{
				IOManager& ioManager = GetIOManager();
				if (isCleanedUp)
				{
					if (FileNode* fileNode = fileContext->GetFileNode())
					{
						auto lock = fileNode->LockShared();

						FileHandle tempHandle(fileNode->GetFullPathWithNS(), AccessRights::GenericWrite, FileShare::All, CreationDisposition::OpenExisting);
						if (tempHandle.IsValid())
						{
							// Need to check if its really needs to be a handle of 'fileContext' and not 'tempFileHandle'.
							return ioManager.WriteFileSync(fileContext->GetHandle(), eventInfo, fileContext);
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					return STATUS_FILE_INVALID;
				}
				else
				{
					if (ioManager.IsAsyncIOEnabled())
					{
						return ioManager.WriteFileAsync(*fileContext, eventInfo);
					}
					else
					{
						return ioManager.WriteFileSync(fileContext->GetHandle(), eventInfo, fileContext);
					}
				}
			}
		}
		return STATUS_FILE_CLOSED;
	}

	NTSTATUS ConvergenceFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* originalNode = fileContext->GetFileNode())
			{
				FileNode* targetNodeParent = nullptr;
				FileNode* targetNode = m_VirtualTree.NavigateToFile(eventInfo.NewFileName, targetNodeParent);

				if (targetNode)
				{
					// We have both files, so overwrite one with another if allowed
					if (eventInfo.ReplaceIfExists)
					{
						auto originalLock = originalNode->LockExclusive();
						auto targetLock = targetNode->LockExclusive();

						if (::MoveFileExW(originalNode->GetFullPathWithNS(), targetNode->GetFullPathWithNS(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING))
						{
							// Move succeeded, take original node's file info
							targetNode->TakeItem(std::move(*originalNode));

							// And remove original file from the tree
							originalNode->RemoveThisChild();

							return STATUS_SUCCESS;
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					else
					{
						return GetNtStatusByWin32ErrorCode(ERROR_ALREADY_EXISTS);
					}
				}
				else if (originalNode->GetParent() == targetNodeParent)
				{
					// We don't have target file, but original node parent is the same as supposed 
					// target parent. So this is actually renaming.

					const KxDynamicStringW newName = KxDynamicStringW(eventInfo.NewFileName).after_last(L'\\');
					if (auto originalLock = originalNode->LockExclusive(); !newName.empty())
					{
						// Save original path and name
						const KxDynamicStringW originalName = originalNode->GetName();
						const KxDynamicStringW originalPath = originalNode->GetFullPathWithNS();

						// Set name to temporary item to get new path
						auto GetNewPath = [originalNode, &newName]()
						{
							KxFileItem item;
							item.SetName(newName);
							item.SetSource(originalNode->GetSource());

							return item.GetFullPathWithNS();
						};
						const KxDynamicStringW newPath = GetNewPath();

						// Rename file system object
						const NTSTATUS status = fileContext->GetHandle().SetPath(newPath, eventInfo.ReplaceIfExists);
						if (status == STATUS_SUCCESS)
						{
							// Rename the node if we successfully renamed file system object
							auto parentLock = targetNodeParent->LockExclusive();
							originalNode->SetName(newName);
						}
						return status;
					}
					return STATUS_OBJECT_NAME_INVALID;
				}
				else
				{
					// We don't have target file, nor it's a rename request, so it's a real move.
					// Branch should be already constructed, so move new node to supposed target parent.
					auto originalLock = originalNode->LockExclusive();
					auto parentLock = targetNodeParent->LockExclusive();

					// Get new path
					auto[newPath, virtualDirectory] = GetTargetPath(targetNode, eventInfo.NewFileName, true);

					// Move the file
					if (::MoveFileExW(originalNode->GetFullPathWithNS(), newPath, MOVEFILE_COPY_ALLOWED))
					{
						FileNode& newNode = targetNodeParent->AddChild(std::make_unique<FileNode>(newPath.get_view(), targetNodeParent), virtualDirectory);

						// Take original node attributes and remove it
						newNode.TakeItem(std::move(*originalNode));
						originalNode->RemoveThisChild();

						return STATUS_SUCCESS;
					}
					return GetNtStatusByWin32LastErrorCode();
				}
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnGetFileInfo(EvtGetFileInfo& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			// Maybe it's better to cache BY_HANDLE_FILE_INFORMATION in virtual tree and just copy it here?
			if (fileContext->GetHandle().GetInfo(eventInfo.FileHandleInfo))
			{
				KxVFS_DebugPrint(L"Successfully retrieved file info by handle for: %s", eventInfo.FileName);
				return STATUS_SUCCESS;
			}
			else if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();
				fileNode->GetItem().ToBY_HANDLE_FILE_INFORMATION(eventInfo.FileHandleInfo);

				KxVFS_DebugPrint(L"Successfully retrieved file info by node for: %s", eventInfo.FileName);
				return STATUS_SUCCESS;
			}

			KxVFS_DebugPrint(L"Couldn't find file node for: %s", eventInfo.FileName);
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxVFS_DebugPrint(L"Enumerating files in directory: %s ", eventInfo.PathName);
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();
				fileNode->WalkChildren([&eventInfo](const FileNode& node)
				{
					return OnFileFound(eventInfo, node.GetItem().AsWIN32_FIND_DATA());
				});
				return STATUS_SUCCESS;
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
	NTSTATUS ConvergenceFS::OnFindStreams(EvtFindStreams & eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxVFS_DebugPrint(L"Enumerating streams for: %s ", eventInfo.FileName);
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();

				WIN32_FIND_STREAM_DATA findData = {0};
				SearchHandle findHandle = ::FindFirstStreamW(fileNode->GetFullPathWithNS(), FindStreamInfoStandard, &findData, 0);
				if (findHandle.IsValid())
				{
					size_t foundStreamsCount = 0;

					Dokany2::DOKAN_STREAM_FIND_RESULT findResult = eventInfo.FillFindStreamData(&eventInfo, &findData);
					if (findResult == Dokany2::DOKAN_STREAM_BUFFER_CONTINUE)
					{
						foundStreamsCount++;
						while (::FindNextStreamW(findHandle, &findData) && (findResult = eventInfo.FillFindStreamData(&eventInfo, &findData)) == Dokany2::DOKAN_STREAM_BUFFER_CONTINUE)
						{
							foundStreamsCount++;
						}
					}
					const DWORD errorCode = ::GetLastError();

					if (findResult == Dokany2::DOKAN_STREAM_BUFFER_FULL)
					{
						// FindStreams returned 'foundStreamsCount' entries in 'node' with STATUS_BUFFER_OVERFLOW
						// https://msdn.microsoft.com/en-us/library/windows/hardware/ff540364(v=vs.85).aspx
						return STATUS_BUFFER_OVERFLOW;
					}
					if (errorCode != ERROR_HANDLE_EOF)
					{
						return GetNtStatusByWin32ErrorCode(errorCode);
					}
					return STATUS_SUCCESS;
				}
				return GetNtStatusByWin32LastErrorCode();
			}
			return STATUS_FILE_INVALID;
		}
		return STATUS_FILE_CLOSED;
	}
}

namespace KxVFS
{
	bool ConvergenceFS::UpdateAttributes(FileContext& fileContext)
	{
		BY_HANDLE_FILE_INFORMATION fileInfo = {};
		if (FileNode* fileNode = fileContext.GetFileNode(); fileNode && fileContext.GetHandle().GetInfo(fileInfo))
		{
			auto lock = fileNode->LockExclusive();

			KxVFS_DebugPrint(L"%s: %s", __FUNCTIONW__, fileNode->GetFullPath().data());
			fileNode->FromBY_HANDLE_FILE_INFORMATION(fileInfo);
			return true;
		}
		return false;
	}
}
