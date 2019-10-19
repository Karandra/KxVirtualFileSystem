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
	void BasicFileSystem::LogDokanyException(uint32_t exceptionCode)
	{
		if (FileSystemService::GetInstance())
		{
			const KxDynamicStringRefW exceptionMessage = Utility::ExceptionCodeToString(exceptionCode);
			KxVFS_Log(LogLevel::Fatal, L"Fatal exception '%1 (%2)' occurred while initializing Dokany", exceptionMessage, exceptionCode);
		}
	}
	uint32_t BasicFileSystem::ConvertDokanyOptions(FSFlags flags)
	{
		uint32_t dokanyOptions = 0;
		auto TestAndSet = [&dokanyOptions, flags](uint32_t dokanyOption, FSFlags flag)
		{
			Utility::ModFlagRef(dokanyOptions, dokanyOption, flags & flag);
		};

		TestAndSet(DOKAN_OPTION_ALT_STREAM, FSFlags::AlternateStream);
		TestAndSet(DOKAN_OPTION_WRITE_PROTECT, FSFlags::WriteProtected);
		TestAndSet(DOKAN_OPTION_NETWORK, FSFlags::NetworkDrive);
		TestAndSet(DOKAN_OPTION_REMOVABLE, FSFlags::RemoveableDrive);
		TestAndSet(DOKAN_OPTION_MOUNT_MANAGER, FSFlags::MountManager);
		TestAndSet(DOKAN_OPTION_CURRENT_SESSION, FSFlags::CurrentSession);
		TestAndSet(DOKAN_OPTION_FILELOCK_USER_MODE, FSFlags::FileLockUserMode);
		TestAndSet(DOKAN_OPTION_FORCE_SINGLE_THREADED, FSFlags::ForceSingleThreaded);
		return dokanyOptions;
	}

	FSError BasicFileSystem::DoMount()
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, !IsMounted() ? L"allowed" : L"disallowed");

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

				// Disable use of stderr log output and enable Dokany's built-in log features if allowed.
				m_Options.Options &= ~DOKAN_OPTION_STDERR;
				m_Options.Options |= Setup::EnableLog && ILogger::IsLogEnabled() ? DOKAN_OPTION_DEBUG : 0;

				// Create file system
				bool isCreated = false;

				// Dokany can throw if mount point is used by another process despite of all the checks above
				// or something else is wrong, so just catch anything and return error.
				SafelyCallDokanyFunction([this, &isCreated]()
				{
					// DOKAN_HANDLE is actually DOKAN_INSTANCE
					Dokany2::DOKAN_HANDLE handle = nullptr;
					isCreated = Dokany2::DokanCreateFileSystem(&m_Options, &m_Operations, &handle) == ToInt(FSErrorCode::Success);
					m_Instance = reinterpret_cast<Dokany2::DOKAN_INSTANCE*>(handle);
				});
				if (!isCreated)
				{
					m_Instance = nullptr;
					return FSErrorCode::Unknown;
				}
				return isCreated;
			}
			return FSErrorCode::InvalidMountPoint;
		}
		return FSErrorCode::CanNotMount;
	}
	bool BasicFileSystem::DoUnMount()
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, IsMounted() ? L"allowed" : L"disallowed");

		if (IsMounted())
		{
			// Here Dokany will call our unmount handler and that handler in turn will call 'OnUnMountInternal'
			return SafelyCallDokanyFunction([this]()
			{
				Dokany2::DokanCloseHandle(m_Instance);
			});
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

	bool BasicFileSystem::IsProcessCreatedInVFS(uint32_t pid) const
	{
		if (IsMounted())
		{
			ProcessHandle process(pid, AccessRights::ProcessQueryLimitedInformation);
			if (process && process.IsActive())
			{
				constexpr size_t prefixLength = 7;
				const size_t deviceNameLength = std::char_traits<wchar_t>::length(m_Instance->DeviceName);

				KxDynamicStringW path = process.GetImagePath();

				// 'DeviceName' string starts with '\Volume' but image name start with '\Device\Volume'
				path.erase(0, prefixLength);
				path.resize(deviceNameLength);
				return path == m_Instance->DeviceName;
			}
		}
		return false;
	}

	NTSTATUS BasicFileSystem::OnMountInternal(EvtMounted& eventInfo)
	{
		m_IsMounted = true;
		m_Service.AddActiveFS(*this);

		return OnMount(eventInfo);
	}
	NTSTATUS BasicFileSystem::OnUnMountInternal(EvtUnMounted& eventInfo)
	{
		KxVFS_Log(LogLevel::Info, __FUNCTIONW__);

		NTSTATUS statusCode = STATUS_UNSUCCESSFUL;
		if (CriticalSectionLocker lock(m_UnmountCS); true)
		{
			KxVFS_Log(LogLevel::Info, L"In EnterCriticalSection: %1", __FUNCTIONW__);

			m_Instance = nullptr;
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
				KxVFS_Log(LogLevel::Info, L"We are destructing, don't call 'OnUnMount'");
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
		KxVFS_Log(LogLevel::Info, L"%1",
				  __FUNCTIONW__
		);

		return (void)GetFromContext(eventInfo->DokanOptions)->OnMountInternal(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_Unmount(EvtUnMounted* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1",
				  __FUNCTIONW__
		);

		return (void)GetFromContext(eventInfo->DokanOptions)->OnUnMountInternal(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1, Requestor Process: %2",
				  __FUNCTIONW__,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnGetVolumeFreeSpace(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1, Requestor Process: %2",
				  __FUNCTIONW__,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnGetVolumeInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1, Requestor Process: %2",
				  __FUNCTIONW__,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnGetVolumeAttributes(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_CreateFile(EvtCreateFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnCreateFile(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_CloseFile(EvtCloseFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", DeleteOnClose: %3, Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->DokanFileInfo->DeleteOnClose,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return (void)GetFromContext(eventInfo)->OnCloseFile(*eventInfo);
	}
	void DOKAN_CALLBACK BasicFileSystem::Dokan_CleanUp(EvtCleanUp* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", DeleteOnClose: %3, Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->DokanFileInfo->DeleteOnClose,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return (void)GetFromContext(eventInfo)->OnCleanUp(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_MoveFile(EvtMoveFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\" -> \"%3\", Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->NewFileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnMoveFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", DeleteOnClose: %3, Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->DokanFileInfo->DeleteOnClose,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnCanDeleteFile(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_LockFile(EvtLockFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnLockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_UnlockFile(EvtUnlockFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnUnlockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnGetFileSecurity(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnSetFileSecurity(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_ReadFile(EvtReadFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", PagingIO: %3, Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->DokanFileInfo->PagingIo,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnReadFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_WriteFile(EvtWriteFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", PagingIO: %3, Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  (bool)eventInfo->DokanFileInfo->PagingIo,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnWriteFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnFlushFileBuffers(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnSetEndOfFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnSetAllocationSize(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_GetFileInfo(EvtGetFileInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnGetFileInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnSetBasicFileInfo(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindFiles(EvtFindFiles* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\\*\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->PathName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnFindFiles(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindFilesWithPattern(EvtFindFilesWithPattern* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\\%3\", Requestor Process: %4",
				  __FUNCTIONW__,
				  eventInfo->PathName,
				  eventInfo->SearchPattern,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnFindFilesWithPattern(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK BasicFileSystem::Dokan_FindStreams(EvtFindStreams* eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: \"%2\", Requestor Process: %3",
				  __FUNCTIONW__,
				  eventInfo->FileName,
				  eventInfo->DokanFileInfo->ProcessId
		);

		return GetFromContext(eventInfo)->OnFindStreams(*eventInfo);
	}
}
