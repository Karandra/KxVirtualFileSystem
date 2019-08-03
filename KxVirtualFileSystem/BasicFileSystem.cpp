/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/BasicFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	FSError BasicFileSystem::DoMount()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA(": ");
		OutputDebugStringA(!IsMounted() ? "allowed" : "disallowed");
		OutputDebugStringA("\r\n");

		if (!IsMounted())
		{
			// Mount point is not a drive, remove folder if empty and create a new one
			if (m_MountPoint.length() > 2)
			{
				::RemoveDirectoryW(m_MountPoint.data());
				Utility::CreateDirectoryTree(m_MountPoint);
			}

			// Allow mount to empty folders
			if (KxFileFinder::IsDirectoryEmpty(m_MountPoint))
			{
				// Update options
				m_Options.ThreadCount = 0; // Dokany 2.x uses system threadpool, so this does nothing
				m_Options.MountPoint = m_MountPoint.data();
				m_Options.Options = ConvertDokanyOptions(m_Flags);
				m_Options.Timeout = 0; // Doesn't seems to affect anything

				// Create file system
				__try
				{
					return Dokany2::DokanCreateFileSystem(&m_Options, &m_Operations, &m_Handle);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					// Dokany can throw if mount point is used by another process despite of all the checks above
					// or something else is wrong, so just catch anything and return error.
					
					OutputDebugStringA(__FUNCTION__);
					OutputDebugStringA(": ");
					OutputDebugStringA("Exception thrown\r\n");

					m_Handle = nullptr;
					return FSErrorCode::Unknown;
				}
			}
			return FSErrorCode::InvalidMountPoint;
		}
		return FSErrorCode::CanNotMount;
	}
	bool BasicFileSystem::DoUnMount()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA(": ");
		OutputDebugStringA(IsMounted() ? "allowed" : "disallowed");
		OutputDebugStringA("\r\n");

		if (IsMounted())
		{
			// Here Dokany will call our unmount handler and that handler in turn will call 'OnUnMountInternal'
			Dokany2::DokanCloseHandle(m_Handle);
			return true;
		}
		return false;
	}

	BasicFileSystem::BasicFileSystem(FileSystemService& service, KxDynamicStringRefW mountPoint, FSFlags flags)
		:m_FileContextManager(*this), m_IOManager(*this), m_Service(service), m_MountPoint(mountPoint), m_Flags(flags)
	{
		// Options
		m_Options.GlobalContext = reinterpret_cast<ULONG64>(static_cast<IFileSystem*>(this));
		m_Options.Version = DOKAN_VERSION;

		// Operations
		m_Operations.Mounted = Dokan_Mount;
		m_Operations.Unmounted = Dokan_Unmount;

		m_Operations.GetVolumeFreeSpace = Dokan_GetVolumeFreeSpace;
		m_Operations.GetVolumeInformationW = Dokan_GetVolumeInfo;
		m_Operations.GetVolumeAttributes = Dokan_GetVolumeAttributes;

		m_Operations.ZwCreateFile = Dokan_CreateFile;
		m_Operations.CloseFile = Dokan_CloseFile;
		m_Operations.Cleanup = Dokan_CleanUp;
		m_Operations.MoveFileW = Dokan_MoveFile;
		m_Operations.CanDeleteFile = Dokan_CanDeleteFile;

		m_Operations.LockFile = Dokan_LockFile;
		m_Operations.UnlockFile = Dokan_UnlockFile;
		m_Operations.GetFileSecurityW = Dokan_GetFileSecurity;
		m_Operations.SetFileSecurityW = Dokan_SetFileSecurity;

		m_Operations.ReadFile = Dokan_ReadFile;
		m_Operations.WriteFile = Dokan_WriteFile;
		m_Operations.FlushFileBuffers = Dokan_FlushFileBuffers;
		m_Operations.SetEndOfFile = Dokan_SetEndOfFile;
		m_Operations.SetAllocationSize = Dokan_SetAllocationSize;
		m_Operations.GetFileInformation = Dokan_GetFileInfo;
		m_Operations.SetFileBasicInformation = Dokan_SetBasicFileInfo;

		m_Operations.FindFiles = Dokan_FindFiles;
		m_Operations.FindFilesWithPattern = Dokan_FindFilesWithPattern;
		m_Operations.FindStreams = Dokan_FindStreams;
	}
	BasicFileSystem::~BasicFileSystem()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		m_IsDestructing = true;
		DoUnMount();
	}

	FSError BasicFileSystem::Mount()
	{
		if (!m_FileContextManager.Init())
		{
			return FSErrorCode::FileContextManagerInitFailed;
		}
		if (m_IOManager.IsAsyncIOEnabled() && !m_IOManager.Init())
		{
			return FSErrorCode::IOManagerInitFialed;
		}
		return DoMount();
	}
	bool BasicFileSystem::UnMount()
	{
		return DoUnMount();
	}

	KxDynamicStringW BasicFileSystem::GetVolumeLabel() const
	{
		return m_Service.GetServiceName();
	}
	KxDynamicStringW BasicFileSystem::GetVolumeFileSystem() const
	{
		// File system name could be anything up to 10 characters.
		// But Windows check few feature availability based on file system name.
		// For this, it is recommended to set NTFS or FAT here.
		return L"NTFS";
	}
	uint32_t BasicFileSystem::GetVolumeSerialNumber() const
	{
		// I assume the volume serial needs to be just a random number,
		// so I think this would suffice. Mirror sample uses '0x19831116' constant.

		#pragma warning (suppress: 4311)
		#pragma warning (suppress: 4302)
		return static_cast<uint32_t>(reinterpret_cast<size_t>(this) ^ reinterpret_cast<size_t>(&m_Service));
	}

	NTSTATUS BasicFileSystem::OnMountInternal(EvtMounted& eventInfo)
	{
		m_IsMounted = true;
		m_Service.AddActiveFS(*this);

		return OnMount(eventInfo);
	}
	NTSTATUS BasicFileSystem::OnUnMountInternal(EvtUnMounted& eventInfo)
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		NTSTATUS statusCode = STATUS_UNSUCCESSFUL;
		if (CriticalSectionLocker lock(m_UnmountCS); true)
		{
			OutputDebugStringA("In EnterCriticalSection: ");
			OutputDebugStringA(__FUNCTION__);
			OutputDebugStringA("\r\n");

			m_Handle = nullptr;
			m_IsMounted = false;
			m_Service.RemoveActiveFS(*this);
			m_FileContextManager.Cleanup();
			m_IOManager.Cleanup();

			if (!m_IsDestructing)
			{
				statusCode = OnUnMount(eventInfo);
			}
			else
			{
				OutputDebugStringA("We are destructing, don't call 'OnUnMount'\r\n");
				statusCode = STATUS_SUCCESS;
			}
		}
		return statusCode;
	}
}

namespace KxVFS
{
	void DOKAN_CALLBACK BasicFileSystem::Dokan_Mount(EvtMounted* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s"), __FUNCTIONW__);
		return (void)GetFromContext(eventInfo->DokanOptions)->OnMountInternal(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_Unmount(EvtUnMounted* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s"), __FUNCTIONW__);
		return (void)GetFromContext(eventInfo->DokanOptions)->OnUnMountInternal(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s"), __FUNCTIONW__);
		return GetFromContext(eventInfo)->OnGetVolumeFreeSpace(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s"), __FUNCTIONW__);
		return GetFromContext(eventInfo)->OnGetVolumeInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s"), __FUNCTIONW__);
		return GetFromContext(eventInfo)->OnGetVolumeAttributes(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_CreateFile(EvtCreateFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnCreateFile(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_CloseFile(EvtCloseFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\", DeleteOnClose: %d"), __FUNCTIONW__, eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCloseFile(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_CleanUp(EvtCleanUp* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\", DeleteOnClose: %d"), __FUNCTIONW__, eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCleanUp(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_MoveFile(EvtMoveFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\" -> \"%s\""), __FUNCTIONW__, eventInfo->FileName, eventInfo->NewFileName);
		return GetFromContext(eventInfo)->OnMoveFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\", DeleteOnClose: %d"), __FUNCTIONW__, eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return GetFromContext(eventInfo)->OnCanDeleteFile(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_LockFile(EvtLockFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnLockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_UnlockFile(EvtUnlockFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnUnlockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileSecurity(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetFileSecurity(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_ReadFile(EvtReadFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnReadFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_WriteFile(EvtWriteFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnWriteFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFlushFileBuffers(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetEndOfFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetAllocationSize(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetFileInfo(EvtGetFileInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetBasicFileInfo(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindFiles(EvtFindFiles* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\\*\""), __FUNCTIONW__, eventInfo->PathName);
		return GetFromContext(eventInfo)->OnFindFiles(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindFilesWithPattern(EvtFindFilesWithPattern* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\\%s\""), __FUNCTIONW__, eventInfo->PathName, eventInfo->SearchPattern);
		return GetFromContext(eventInfo)->OnFindFilesWithPattern(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindStreams(EvtFindStreams* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, TEXT("%s: \"%s\""), __FUNCTIONW__, eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFindStreams(*eventInfo);
	}
}
