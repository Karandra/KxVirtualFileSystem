#include "stdafx.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ConvergenceFS.h"

namespace KxVFS
{
	DynamicStringW ConvergenceFS::MakeFilePath(DynamicStringRefW baseDirectory, DynamicStringRefW requestedPath, bool addNamespace) const
	{
		DynamicStringW outPath = addNamespace ? Utility::GetLongPathPrefix() : NullDynamicStringW;
		outPath += baseDirectory;
		outPath += requestedPath;

		return outPath;
	}
	std::tuple<DynamicStringW, DynamicStringRefW> ConvergenceFS::GetTargetPath(const FileNode* node, DynamicStringRefW requestedPath, bool addNamespace) const
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
	DynamicStringW ConvergenceFS::DispatchLocationRequest(DynamicStringRefW requestedPath)
	{
		return std::get<0>(GetTargetPath(m_VirtualTree.NavigateToAny(requestedPath), requestedPath, true));
	}

	bool ConvergenceFS::ProcessDeleteOnClose(Dokany2::DOKAN_FILE_INFO& fileInfo, FileNode& fileNode) const
	{
		if (fileInfo.DeleteOnClose)
		{
			// Should already be deleted by 'CloseHandle' if opened with 'FILE_FLAG_DELETE_ON_CLOSE'
			if (auto parentLock = fileNode.GetParent()->LockExclusive(); true)
			{
				bool success = false;
				DynamicStringW fullPath = fileNode.GetFullPathWithNS();
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
	ConvergenceFS::ConvergenceFS(FileSystemService& service, DynamicStringRefW mountPoint, DynamicStringRefW writeTarget, FSFlags flags)
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

	void ConvergenceFS::AddVirtualFolder(DynamicStringRefW path)
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
		for (const DynamicStringW& path: m_VirtualFolders)
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
			const DynamicStringW rootPath = rootNode.GetRelativePath();

			for (auto it = m_VirtualFolders.rbegin(); it != m_VirtualFolders.rend(); ++it)
			{
				auto nodeIt = virtualNodes.find(*it);
				const DynamicStringW& folderPath = nodeIt->first;
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
		Utility::CallAtScopeExit atExit([this]()
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
	NtStatus ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"Trying to create/open file or directory: %1", eventInfo.FileName);

		// Paths and nodes
		FileNode* parentNode = nullptr;
		FileNode* targetNode = nullptr;
		if (auto lock = m_VirtualTree.LockShared(); true)
		{
			targetNode = m_VirtualTree.NavigateToAny(eventInfo.FileName, parentNode);
		}

		// Attributes and flags
		const FlagSet<KernelFileOptions> kernelOptions = FromInt<KernelFileOptions>(eventInfo.CreateOptions);

		// Correct flags
		if (targetNode)
		{
			KxVFS_Log(LogLevel::Info, L"Target location: %1", targetNode->GetFullPath());

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
					return NtStatus::FileIsADirectory;
				}
			}

			// Sometimes 'DokanFileInfo.IsDirectory' is set to TRUE but 'DokanFileInfo::FileName' points to a file.
			// I'm not sure if I really need to handle this case. Everything seems to be working without this correction.
			#if 0
			if (targetNode->IsFile() && eventInfo.DokanFileInfo->IsDirectory)
			{
				KxVFS_Log(LogLevel::Info, L"Object \"%s\" is file, but opening as directory is requested. Changing request parameter.", targetNode->GetFullPathWithNS().data());
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
	NtStatus ConvergenceFS::OnCreateFile(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode)
	{
		KxVFS_Log(LogLevel::Info, L"Trying to create/open file: %1", eventInfo.FileName);

		// Flags and attributes
		const FlagSet<FileAttributes> fileAttributes = targetNode ? targetNode->GetAttributes() : FileAttributes::Invalid;
		const FlagSet<FileShare> fileShareOptions = FromInt<FileShare>(eventInfo.ShareAccess);
		auto[requestAttributes, creationDisposition, genericDesiredAccess] = MapKernelToUserCreateFileFlags(eventInfo);
		
		const IOManager& ioManager = GetIOManager();
		if (ioManager.IsAsyncIOEnabled())
		{
			requestAttributes |= FileAttributes::FlagOverlapped;
		}

		// Can't open non-existing file
		if (!targetNode && (creationDisposition == CreationDisposition::OpenExisting || creationDisposition == CreationDisposition::TruncateExisting))
		{
			return NtStatus::ObjectPathNotFound;
		}

		// Cannot overwrite a hidden or system file if flag not set
		if (!CheckAttributesToOverwriteFile(fileAttributes, requestAttributes, creationDisposition))
		{
			return NtStatus::AccessDenied;
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
			KxVFS_Log(LogLevel::Info, L"Attempt to create a file in non-existent directory tree in write target: %1", eventInfo.FileName);

			::SetLastError(ERROR_SUCCESS);
			DynamicStringW folderPath = DynamicStringW(eventInfo.FileName).before_last(L'\\');
			KxVFS_Log(LogLevel::Info, L"Creating directory tree in write target: %1", folderPath);

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
					return NtStatus::InternalError;
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
				::SetFileAttributesW(targetPath, (requestAttributes|fileAttributes).ToInt());
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
						// that the file open and not created by returning NtStatus::ObjectNameCollision
						return NtStatus::ObjectNameCollision;
					}
				}
			}
			else
			{
				::SetLastError(ERROR_INTERNAL_ERROR);
				return NtStatus::InternalError;
			}
			return NtStatus::Success;
		}
		else
		{
			KxVFS_Log(LogLevel::Info, L"Failed to create/open file: %s", targetPath);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}
	NtStatus ConvergenceFS::OnCreateDirectory(EvtCreateFile& eventInfo, FileNode* targetNode, FileNode* parentNode)
	{
		KxVFS_Log(LogLevel::Info, L"Trying to create/open directory: %1", eventInfo.FileName);

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
			return NtStatus::ObjectPathNotFound;
		}

		// Get directory path
		auto[targetPath, virtualDirectory] = GetTargetPath(targetNode, eventInfo.FileName, true);

		SecurityObject newDirectorySecurity = CreateSecurityIfEnabled(eventInfo, targetPath, creationDisposition);
		TokenHandle userTokenHandle = ImpersonateCallerUserIfEnabled(eventInfo);

		// Create folder if it's not exist
		if (!targetNode && (creationDisposition == CreationDisposition::CreateNew || creationDisposition == CreationDisposition::OpenAlways))
		{
			ImpersonateLoggedOnUserIfEnabled(userTokenHandle);

			NtStatus statusCode = NtStatus::Success;
			Utility::CallAtScopeExit atExit([this, &userTokenHandle, &statusCode]()
			{
				CleanupImpersonateCallerUserIfEnabled(userTokenHandle, statusCode);
			});

			if (Utility::CreateDirectoryTreeEx(virtualDirectory, eventInfo.FileName, &newDirectorySecurity.GetAttributes()))
			{
				if (parentNode == nullptr)
				{
					::SetLastError(ERROR_INTERNAL_ERROR);
					return NtStatus::InternalError;
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
					if (statusCode != NtStatus::Success)
					{
						return statusCode;
					}
				}
			}
		}

		// Check first if we're trying to open a file as a directory.
		if (targetNode && !targetNode->IsDirectory() && (eventInfo.CreateOptions & FILE_DIRECTORY_FILE))
		{
			KxVFS_Log(LogLevel::Info, L"Attempt to open a file as a directory: %1", targetPath);
			return NtStatus::NotADirectory;
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

		if (directoryHandle)
		{
			// Directory node should absolutely exist at this point
			if (!targetNode)
			{
				::SetLastError(ERROR_INTERNAL_ERROR);
				return NtStatus::InternalError;
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
				return NtStatus::InternalError;
			}

			// Open succeed but we need to inform the driver that the directory was open and not created
			// by returning NtStatus::ObjectNameCollision
			if (targetNode && creationDisposition == CreationDisposition::OpenAlways)
			{
				return NtStatus::ObjectNameCollision;
			}
			return NtStatus::Success;
		}
		else
		{
			KxVFS_Log(LogLevel::Info, L"Failed to create/open directory: %1", targetPath);
			return GetNtStatusByWin32ErrorCode(errorCode);
		}
	}

	NtStatus ConvergenceFS::OnCanDeleteFile(EvtCanDeleteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();

				if (fileNode->IsReadOnly())
				{
					return NtStatus::CannotDelete;
				}
				if (fileNode->IsDirectory() && fileNode->HasChildren())
				{
					return NtStatus::DirectoryNotEmpty;
				}
				return NtStatus::Success;
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnCloseFile(EvtCloseFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				if (auto contextLock = fileContext->LockExclusive(); true)
				{
					fileContext->MarkClosed();
					if (fileContext->GetHandle())
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
				return NtStatus::Success;
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnCleanUp(EvtCleanUp& eventInfo)
	{
		// Look for comment in 'MirrorFS::OnCleanUp' for some important details
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				if (auto contextLock = fileContext->LockExclusive(); true)
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
				return NtStatus::Success;
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}

	NtStatus ConvergenceFS::OnReadFile(EvtReadFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isClosed = false;
			bool isCleanedUp = false;
			fileContext->InterlockedGetState(isClosed, isCleanedUp);

			if (!isClosed)
			{
				if (isCleanedUp)
				{
					if (FileNode* fileNode = fileContext->GetFileNode())
					{
						auto lock = fileNode->LockShared();

						FileHandle tempHandle(fileNode->GetFullPathWithNS(), AccessRights::GenericRead, FileShare::All, CreationDisposition::OpenExisting);
						if (tempHandle)
						{
							return GetIOManager().ReadFileSync(tempHandle, eventInfo, fileContext);
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					return NtStatus::FileInvalid;
				}
				else
				{
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
			}
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnWriteFile(EvtWriteFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			bool isClosed = false;
			bool isCleanedUp = false;
			fileContext->InterlockedGetState(isClosed, isCleanedUp);

			if (!isClosed)
			{
				if (isCleanedUp)
				{
					if (FileNode* fileNode = fileContext->GetFileNode())
					{
						auto lock = fileNode->LockShared();

						FileHandle tempHandle(fileNode->GetFullPathWithNS(), AccessRights::GenericWrite, FileShare::All, CreationDisposition::OpenExisting);
						if (tempHandle)
						{
							// Need to check if its really needs to be a handle of 'fileContext' and not 'tempHandle'.
							return GetIOManager().WriteFileSync(fileContext->GetHandle(), eventInfo, fileContext);
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					return NtStatus::FileInvalid;
				}
				else
				{
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
			}
		}
		return NtStatus::FileClosed;
	}

	NtStatus ConvergenceFS::OnMoveFile(EvtMoveFile& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			if (FileNode* sourceNode = fileContext->GetFileNode())
			{
				FileNode* targetNodeParent = nullptr;
				FileNode* targetNode = m_VirtualTree.NavigateToFile(eventInfo.NewFileName, targetNodeParent);

				KxVFS_Log(LogLevel::Info, L"%1: \"%2\" -> \"%3\" (ReplaceIfExists: %4), Target parent: %5",
						  __FUNCTIONW__,
						  sourceNode->GetFullPath(),
						  targetNode ? targetNode->GetFullPath() : L"<null>",
						  (bool)eventInfo.ReplaceIfExists,
						  targetNodeParent ? targetNodeParent->GetFullPath() : L"<null>");

				if (targetNode)
				{
					// We have both files, so overwrite one with another if allowed
					KxVFS_Log(LogLevel::Info, L"Direct move: \"%1\" -> \"%2\"", sourceNode->GetFullPathWithNS(), targetNode->GetFullPathWithNS());

					if (eventInfo.ReplaceIfExists)
					{
						auto sourceLock = sourceNode->LockExclusive();
						auto targetLock = targetNode->LockExclusive();

						if (::MoveFileExW(sourceNode->GetFullPathWithNS(), targetNode->GetFullPathWithNS(), MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING))
						{
							// Move succeeded, move source node's file info into the target
							targetNode->TakeItem(std::move(*sourceNode));

							// And remove source file from the tree
							sourceNode->RemoveThisChild();

							return NtStatus::Success;
						}
						return GetNtStatusByWin32LastErrorCode();
					}
					else
					{
						return GetNtStatusByWin32ErrorCode(ERROR_ALREADY_EXISTS);
					}
				}
				else if (sourceNode->GetParent() == targetNodeParent)
				{
					// We don't have target file, but source node parent is the same as supposed 
					// target parent. So this is actually renaming.
					const DynamicStringW newName = DynamicStringW(eventInfo.NewFileName).after_last(L'\\');
					KxVFS_Log(LogLevel::Info, L"New file name: \"%1\"", newName);

					if (auto originalLock = sourceNode->LockExclusive(); !newName.empty())
					{
						// Set name to temporary item to construct a new path
						const DynamicStringW newPath = [sourceNode, &newName]()
						{
							FileItem item;
							item.SetName(newName);
							item.SetSource(sourceNode->GetSource());

							return item.GetFullPathWithNS();
						}();

						// Rename file system object
						KxVFS_Log(LogLevel::Info, L"Renaming: \"%1\" -> \"%2\"", sourceNode->GetFullPathWithNS(), newPath);

						const NtStatus status = fileContext->GetHandle().SetPath(newPath, eventInfo.ReplaceIfExists);
						if (status == NtStatus::Success)
						{
							// Rename the node if we successfully renamed its file system object
							auto parentLock = targetNodeParent->LockExclusive();
							sourceNode->SetName(newName);
						}
						return status;
					}
					return NtStatus::ObjectNameInvalid;
				}
				else
				{
					// We don't have target file, nor it's a rename request, so it's a move to a completely new location.
					// Branch should be already constructed, so move a new node to a supposed target parent.
					auto sourceLock = sourceNode->LockExclusive();
					auto parentLock = targetNodeParent->LockExclusive();

					// Get new target path
					const auto [newTargetPath, virtualDirectory] = GetTargetPath(targetNode, eventInfo.NewFileName, true);

					// Move the file
					KxVFS_Log(LogLevel::Info, L"Moving file to a new location: \"%1\" -> \"%2\"", sourceNode->GetFullPathWithNS(), newTargetPath);

					// Target directory tree might not exist yet. Try to move the file and, if directory doesn't exist, create it and repeat.
					auto DoMoveFile = [&]()
					{
						return ::MoveFileExW(sourceNode->GetFullPathWithNS(), newTargetPath, MOVEFILE_COPY_ALLOWED);
					};

					bool isMoved = DoMoveFile();
					DWORD errorCode = ::GetLastError();
					if (!isMoved && errorCode == ERROR_PATH_NOT_FOUND)
					{
						// Source is definitely exist so it must be target directory doesn't exist. Create it.
						KxVFS_Log(LogLevel::Info, L"Target directory tree doesn't exist, creating.");

						Utility::CreateDirectoryTree(newTargetPath, true);
						isMoved = DoMoveFile();
						errorCode = ::GetLastError();
					}
					if (isMoved)
					{
						FileNode& newNode = targetNodeParent->AddChild(std::make_unique<FileNode>(newTargetPath.get_view(), targetNodeParent), virtualDirectory);

						// Move source node attributes to the new node and remove the source
						newNode.TakeItem(std::move(*sourceNode));
						sourceNode->RemoveThisChild();

						KxVFS_Log(LogLevel::Info, L"Successfully moved to: %1", newNode.GetFullPath());
						return NtStatus::Success;
					}
					return GetNtStatusByWin32ErrorCode(errorCode);
				}
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnGetFileInfo(EvtGetFileInfo& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			// Maybe it's better to cache BY_HANDLE_FILE_INFORMATION in virtual tree and just copy it here?
			if (fileContext->GetHandle().GetInfo(eventInfo.FileHandleInfo))
			{
				KxVFS_Log(LogLevel::Info, L"Successfully retrieved file info by handle for: %1", eventInfo.FileName);
				return NtStatus::Success;
			}
			else if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();
				fileNode->GetItem().ToBY_HANDLE_FILE_INFORMATION(eventInfo.FileHandleInfo);

				KxVFS_Log(LogLevel::Info, L"Successfully retrieved file info by node for: %1", eventInfo.FileName);
				return NtStatus::Success;
			}

			KxVFS_Log(LogLevel::Info, L"Couldn't find file node for: %1", eventInfo.FileName);
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	
	NtStatus ConvergenceFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxVFS_Log(LogLevel::Info, L"Enumerating files in directory: '%1'", eventInfo.PathName);
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();
				fileNode->WalkChildren([&eventInfo](const FileNode& node)
				{
					return OnFileFound(eventInfo, node.GetItem().AsWIN32_FIND_DATA());
				});
				KxVFS_Log(LogLevel::Info, L"Found %1 files", fileNode->GetChildrenCount());

				return NtStatus::Success;
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnFindFilesWithPattern(EvtFindFilesWithPattern& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxVFS_Log(LogLevel::Info, L"Enumerating files in directory: '%1' with filtering by: '%2'", eventInfo.PathName, eventInfo.SearchPattern);
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();

				size_t foundCount = 0;
				DynamicStringW pattern = Utility::StringToLower(eventInfo.SearchPattern);
				fileNode->WalkChildren([&eventInfo, &pattern, &foundCount](const FileNode& node)
				{
					const DynamicStringRefW name = node.GetNameLC();
					if (Dokany2::DokanIsNameInExpression(pattern.data(), name.data(), FALSE))
					{
						OnFileFound(eventInfo, node.GetItem().AsWIN32_FIND_DATA());
						foundCount++;
					}
					return true;
				});
				KxVFS_Log(LogLevel::Info, L"Found %1 files", foundCount);

				return NtStatus::Success;
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
	}
	NtStatus ConvergenceFS::OnFindStreams(EvtFindStreams& eventInfo)
	{
		if (FileContext* fileContext = GetFileContext(eventInfo))
		{
			KxVFS_Log(LogLevel::Info, L"Enumerating streams for: '%1'", eventInfo.FileName);
			if (FileNode* fileNode = fileContext->GetFileNode())
			{
				auto lock = fileNode->LockShared();

				WIN32_FIND_STREAM_DATA findData = {0};
				SearchHandle findHandle = ::FindFirstStreamW(fileNode->GetFullPathWithNS(), FindStreamInfoStandard, &findData, 0);
				if (findHandle)
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

					KxVFS_Log(LogLevel::Info, L"Found %1 streams", foundStreamsCount);
					if (findResult == Dokany2::DOKAN_STREAM_BUFFER_FULL)
					{
						// FindStreams returned 'foundStreamsCount' entries in 'node' with NtStatus::BufferOverflow
						// https://msdn.microsoft.com/en-us/library/windows/hardware/ff540364(v=vs.85).aspx
						return NtStatus::BufferOverflow;
					}
					if (errorCode != ERROR_HANDLE_EOF)
					{
						return GetNtStatusByWin32ErrorCode(errorCode);
					}
					return NtStatus::Success;
				}
				return GetNtStatusByWin32LastErrorCode();
			}
			return NtStatus::FileInvalid;
		}
		return NtStatus::FileClosed;
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

			KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, fileNode->GetFullPath());
			fileNode->FromBY_HANDLE_FILE_INFORMATION(fileInfo);
			return true;
		}
		return false;
	}
}
