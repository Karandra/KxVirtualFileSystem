#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "Utility/KxVFSUtility.h"

bool KxVFSBase::UnMountDirectory(const WCHAR* sMountPoint)
{
	return DokanRemoveMountPoint(sMountPoint);
}
bool KxVFSBase::IsCodeSuccess(int nErrorCode)
{
	return DOKAN_SUCCEEDED(nErrorCode);
}
KxDynamicString KxVFSBase::GetErrorCodeMessage(int nErrorCode)
{
	const WCHAR* sMessage = NULL;
	switch (nErrorCode)
	{
		case DOKAN_SUCCESS:
		{
			sMessage = L"Success";
			break;
		}
		case DOKAN_ERROR:
		{
			sMessage = L"Mount error";
			break;
		}
		case DOKAN_DRIVE_LETTER_ERROR:
		{
			sMessage = L"Bad Drive letter";
			break;
		}
		case DOKAN_DRIVER_INSTALL_ERROR:
		{
			sMessage = L"Can't install driver";
			break;
		}
		case DOKAN_START_ERROR:
		{
			sMessage = L"Driver answer that something is wrong";
			break;
		}
		case DOKAN_MOUNT_ERROR:
		{
			sMessage = L"Can't assign a drive letter or mount point, probably already used by another volume";
			break;
		}
		case DOKAN_MOUNT_POINT_ERROR:
		{
			sMessage = L"Mount point is invalid";
			break;
		}
		case DOKAN_VERSION_ERROR:
		{
			sMessage = L"Requested an incompatible version";
			break;
		}
		default:
		{
			return KxDynamicString::Format(L"Unknown error: %d", nErrorCode);
		}
	};
	return sMessage;
}
size_t KxVFSBase::WriteString(const WCHAR* sSource, WCHAR* sDestination, size_t nMaxDestLength)
{
	size_t nMaxNameLength = nMaxDestLength * sizeof(WCHAR);
	size_t nNameLength = std::min(nMaxNameLength, wcslen(sSource) * sizeof(WCHAR));
	memcpy_s(sDestination, nMaxNameLength, sSource, nNameLength);

	return nNameLength / sizeof(WCHAR);
}

//////////////////////////////////////////////////////////////////////////
void KxVFSBase::SetMounted(bool bValue)
{
	m_IsMounted = bValue;
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

KxVFSBase::KxVFSBase(KxVFSService* pVFSService, const WCHAR* sMountPoint, ULONG nFalgs, ULONG nRequestTimeout)
	:m_ServiceInstance(pVFSService), m_MountPoint(sMountPoint)
{
	// Options
	m_Options.GlobalContext = reinterpret_cast<ULONG64>(this);
	m_Options.Version = DOKAN_VERSION;
	m_Options.ThreadCount = 0;
	m_Options.MountPoint = GetMountPoint();
	m_Options.Options = nFalgs;
	m_Options.Timeout = nRequestTimeout;

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
bool KxVFSBase::SetMountPoint(const WCHAR* sMountPoint)
{
	if (!IsMounted())
	{
		m_MountPoint = sMountPoint;
		return true;
	}
	return false;
}

ULONG KxVFSBase::GetFlags() const
{
	return m_Options.Options;
}
bool KxVFSBase::SetFlags(ULONG nFalgs)
{
	if (!IsMounted())
	{
		m_Options.Options = nFalgs;
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

NTSTATUS KxVFSBase::OnMountInternal(DOKAN_MOUNTED_INFO* pEventInfo)
{
	SetMounted(true);
	GetService()->AddVFS(this);

	return OnMount(pEventInfo);
}
NTSTATUS KxVFSBase::OnUnMountInternal(DOKAN_UNMOUNTED_INFO* pEventInfo)
{
	OutputDebugStringA(__FUNCTION__);
	OutputDebugStringA("\r\n");

	NTSTATUS nStatusCode = STATUS_UNSUCCESSFUL;
	KxVFSCriticalSectionLocker lock(m_UnmountCS);
	{
		OutputDebugStringA("In EnterCriticalSection: ");
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		SetMounted(false);
		GetService()->RemoveVFS(this);

		nStatusCode = OnUnMount(pEventInfo);
	}
	return nStatusCode;
}

//////////////////////////////////////////////////////////////////////////
//#define THIS(pEventInfo) GetFromContext((pEventInfo)->DokanFileInfo->DokanOptions)

void DOKAN_CALLBACK KxVFSBase::Dokan_Mount(DOKAN_MOUNTED_INFO* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return (void)GetFromContext(pEventInfo->DokanOptions)->OnMountInternal(pEventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_Unmount(DOKAN_UNMOUNTED_INFO* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return (void)GetFromContext(pEventInfo->DokanOptions)->OnUnMountInternal(pEventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeFreeSpace(DOKAN_GET_DISK_FREE_SPACE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(pEventInfo)->OnGetVolumeFreeSpace(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeInfo(DOKAN_GET_VOLUME_INFO_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(pEventInfo)->OnGetVolumeInfo(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetVolumeAttributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
	return GetFromContext(pEventInfo)->OnGetVolumeAttributes(pEventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_CreateFile(DOKAN_CREATE_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnCreateFile(pEventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_CloseFile(DOKAN_CLOSE_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName, (int)pEventInfo->DokanFileInfo->DeleteOnClose);
	return (void)GetFromContext(pEventInfo)->OnCloseFile(pEventInfo);
}
void DOKAN_CALLBACK KxVFSBase::Dokan_CleanUp(DOKAN_CLEANUP_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName, (int)pEventInfo->DokanFileInfo->DeleteOnClose);
	return (void)GetFromContext(pEventInfo)->OnCleanUp(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_MoveFile(DOKAN_MOVE_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\" -> \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName, pEventInfo->NewFileName);
	return GetFromContext(pEventInfo)->OnMoveFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_CanDeleteFile(DOKAN_CAN_DELETE_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName, (int)pEventInfo->DokanFileInfo->DeleteOnClose);
	return GetFromContext(pEventInfo)->OnCanDeleteFile(pEventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_LockFile(DOKAN_LOCK_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnLockFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_UnlockFile(DOKAN_UNLOCK_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnUnlockFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetFileSecurity(DOKAN_GET_FILE_SECURITY_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnGetFileSecurity(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetFileSecurity(DOKAN_SET_FILE_SECURITY_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnSetFileSecurity(pEventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_ReadFile(DOKAN_READ_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnReadFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_WriteFile(DOKAN_WRITE_FILE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnWriteFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FlushFileBuffers(DOKAN_FLUSH_BUFFERS_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnFlushFileBuffers(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetEndOfFile(DOKAN_SET_EOF_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnSetEndOfFile(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetAllocationSize(DOKAN_SET_ALLOCATION_SIZE_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnSetAllocationSize(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_GetFileInfo(DOKAN_GET_FILE_INFO_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnGetFileInfo(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_SetBasicFileInfo(DOKAN_SET_FILE_BASIC_INFO_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnSetBasicFileInfo(pEventInfo);
}

NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FindFiles(DOKAN_FIND_FILES_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\\*\"\r\n"), TEXT(__FUNCTION__), pEventInfo->PathName);
	return GetFromContext(pEventInfo)->OnFindFiles(pEventInfo);
}
NTSTATUS DOKAN_CALLBACK KxVFSBase::Dokan_FindStreams(DOKAN_FIND_STREAMS_EVENT* pEventInfo)
{
	KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), pEventInfo->FileName);
	return GetFromContext(pEventInfo)->OnFindStreams(pEventInfo);
}
