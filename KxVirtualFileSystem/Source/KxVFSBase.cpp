#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "Utility/KxVFSUtility.h"

bool KxVFSBase::UnMountDirectory(const WCHAR* mountPoint)
{
	return DokanRemoveMountPoint(mountPoint);
}
bool KxVFSBase::IsCodeSuccess(int errorCode)
{
	return DOKAN_SUCCEEDED(errorCode);
}
KxDynamicString KxVFSBase::GetErrorCodeMessage(int errorCode)
{
	const WCHAR* message = NULL;
	switch (errorCode)
	{
		case DOKAN_SUCCESS:
		{
			message = L"Success";
			break;
		}
		case DOKAN_ERROR:
		{
			message = L"Mount error";
			break;
		}
		case DOKAN_DRIVE_LETTER_ERROR:
		{
			message = L"Bad Drive letter";
			break;
		}
		case DOKAN_DRIVER_INSTALL_ERROR:
		{
			message = L"Can't install driver";
			break;
		}
		case DOKAN_START_ERROR:
		{
			message = L"Driver answer that something is wrong";
			break;
		}
		case DOKAN_MOUNT_ERROR:
		{
			message = L"Can't assign a drive letter or mount point, probably already used by another volume";
			break;
		}
		case DOKAN_MOUNT_POINT_ERROR:
		{
			message = L"Mount point is invalid";
			break;
		}
		case DOKAN_VERSION_ERROR:
		{
			message = L"Requested an incompatible version";
			break;
		}
		default:
		{
			return KxDynamicString::Format(L"Unknown error: %d", errorCode);
		}
	};
	return message;
}
size_t KxVFSBase::WriteString(const WCHAR* source, WCHAR* destination, size_t maxDestLength)
{
	size_t maxNameLength = maxDestLength * sizeof(WCHAR);
	size_t nameLength = std::min(maxNameLength, wcslen(source) * sizeof(WCHAR));
	memcpy_s(destination, maxNameLength, source, nameLength);

	return nameLength / sizeof(WCHAR);
}

//////////////////////////////////////////////////////////////////////////
void KxVFSBase::SetMounted(bool value)
{
	m_IsMounted = value;
}
int KxVFSBase::DoMount()
{
	OutputDebugStringA(__FUNCTION__);
	OutputDebugStringA(": ");
	OutputDebugStringA(IsMounted() ? "allowed" : "disallowed");
	OutputDebugStringA("\r\n");

	if (!IsMounted())
	{
		// Mount point is not a drive, remove folder if empty and create a new one
		if (wcslen(GetMountPoint()) > 2)
		{
			::RemoveDirectoryW(GetMountPoint());
			KxVFSUtility::CreateFolderTree(GetMountPoint());
		}

		// Allow mount to empty folders
		if (KxVFSUtility::IsFolderEmpty(GetMountPoint()))
		{
			return DokanCreateFileSystem(&m_Options, &m_Operations, &m_Handle);
		}
		return DOKAN_MOUNT_ERROR;
	}
	return DOKAN_ERROR;
}
bool KxVFSBase::DoUnMount()
{
	OutputDebugStringA(__FUNCTION__);
	OutputDebugStringA(": ");
	OutputDebugStringA(IsMounted() ? "allowed" : "disallowed");
	OutputDebugStringA("\r\n");

	if (IsMounted())
	{
		DokanCloseHandle(m_Handle);
		return true;
	}
	return false;
}

KxVFSBase::KxVFSBase(KxVFSService* vfsService, const WCHAR* mountPoint, ULONG falgs, ULONG requestTimeout)
	:m_ServiceInstance(vfsService), m_MountPoint(mountPoint)
{
	// Options
	m_Options.GlobalContext = reinterpret_cast<ULONG64>(this);
	m_Options.Version = DOKAN_VERSION;
	m_Options.ThreadCount = 0;
	m_Options.MountPoint = GetMountPoint();
	m_Options.Options = falgs;
	m_Options.Timeout = requestTimeout;

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
	m_Operations.FindFilesWithPattern = NULL; // Overriding FindFiles is enough
	m_Operations.FindStreams = Dokan_FindStreams;
}
KxVFSBase::~KxVFSBase()
{
	OutputDebugStringA(__FUNCTION__);
	OutputDebugStringA("\r\n");

	DoUnMount();
}

int KxVFSBase::Mount()
{
	return DoMount();
}
bool KxVFSBase::UnMount()
{
	return DoUnMount();
}

const WCHAR* KxVFSBase::GetVolumeName() const
{
	return GetService()->GetServiceName();
}
const WCHAR* KxVFSBase::GetVolumeFileSystemName() const
{
	// File system name could be anything up to 10 characters.
	// But Windows check few feature availability based on file system name.
	// For this, it is recommended to set NTFS or FAT here.
	return L"NTFS";
}
ULONG KxVFSBase::GetVolumeSerialNumber() const
{
	// I assume the volume serial needs to be just a random number
	// so I think this would suffice. Mirror sample uses constant 0x19831116.

	#pragma warning (suppress: 4311)
	#pragma warning (suppress: 4302)
	return (ULONG)(reinterpret_cast<size_t>(this) ^ reinterpret_cast<size_t>(GetService()));
}

KxVFSService* KxVFSBase::GetService()
{
	return m_ServiceInstance;
}
const KxVFSService* KxVFSBase::GetService() const
{
	return m_ServiceInstance;
}

bool KxVFSBase::IsMounted() const
{
	return m_IsMounted;
}
bool KxVFSBase::SetMountPoint(const WCHAR* mountPoint)
{
	if (!IsMounted())
	{
		m_MountPoint = mountPoint;
		return true;
	}
	return false;
}

ULONG KxVFSBase::GetFlags() const
{
	return m_Options.Options;
}
bool KxVFSBase::SetFlags(ULONG falgs)
{
	if (!IsMounted())
	{
		m_Options.Options = falgs;
		return true;
	}
	return false;
}

NTSTATUS KxVFSBase::GetNtStatusByWin32ErrorCode(DWORD nWin32ErrorCode) const
{
	return DokanNtStatusFromWin32(nWin32ErrorCode);
}
NTSTATUS KxVFSBase::GetNtStatusByWin32LastErrorCode() const
{
	return DokanNtStatusFromWin32(::GetLastError());
}

NTSTATUS KxVFSBase::OnMountInternal(DOKAN_MOUNTED_INFO* eventInfo)
{
	SetMounted(true);
	GetService()->AddVFS(this);

	return OnMount(eventInfo);
}
NTSTATUS KxVFSBase::OnUnMountInternal(DOKAN_UNMOUNTED_INFO* eventInfo)
{
	OutputDebugStringA(__FUNCTION__);
	OutputDebugStringA("\r\n");

	NTSTATUS statusCode = STATUS_UNSUCCESSFUL;
	KxVFSCriticalSectionLocker lock(m_UnmountCS);
	{
		OutputDebugStringA("In EnterCriticalSection: ");
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		SetMounted(false);
		GetService()->RemoveVFS(this);

		statusCode = OnUnMount(eventInfo);
	}
	return statusCode;
}

//////////////////////////////////////////////////////////////////////////
//#define THIS(eventInfo) GetFromContext((eventInfo)->DokanFileInfo->DokanOptions)

void DOKAN_CALLBACK KxVFSBase::Dokan_Mount(DOKAN_MOUNTED_INFO* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return (void)GetFromContext(eventInfo->DokanOptions)->OnMountInternal(eventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_Unmount(DOKAN_UNMOUNTED_INFO* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return (void)GetFromContext(eventInfo->DokanOptions)->OnUnMountInternal(eventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(eventInfo)->OnGetVolumeFreeSpace(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(eventInfo)->OnGetVolumeInfo(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(eventInfo)->OnGetVolumeAttributes(eventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_CreateFile(DOKAN_CREATE_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnCreateFile(eventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_CloseFile(DOKAN_CLOSE_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
	return (void)GetFromContext(eventInfo)->OnCloseFile(eventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_CleanUp(DOKAN_CLEANUP_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
	return (void)GetFromContext(eventInfo)->OnCleanUp(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_MoveFile(DOKAN_MOVE_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\" -> \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, eventInfo->NewFileName);
	return GetFromContext(eventInfo)->OnMoveFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_CanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
	return GetFromContext(eventInfo)->OnCanDeleteFile(eventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_LockFile(DOKAN_LOCK_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnLockFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_UnlockFile(DOKAN_UNLOCK_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnUnlockFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnGetFileSecurity(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnSetFileSecurity(eventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_ReadFile(DOKAN_READ_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnReadFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_WriteFile(DOKAN_WRITE_FILE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnWriteFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnFlushFileBuffers(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetEndOfFile(DOKAN_SET_EOF_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnSetEndOfFile(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnSetAllocationSize(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetFileInfo(DOKAN_GET_FILE_INFO_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnGetFileInfo(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnSetBasicFileInfo(eventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FindFiles(DOKAN_FIND_FILES_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\\*\"\r\n"), TEXT(__FUNCTION__), eventInfo->PathName);
	return GetFromContext(eventInfo)->OnFindFiles(eventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FindStreams(DOKAN_FIND_STREAMS_EVENT* eventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
	return GetFromContext(eventInfo)->OnFindStreams(eventInfo);
}
