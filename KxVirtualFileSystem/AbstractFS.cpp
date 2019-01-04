/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	bool AbstractFS::UnMountDirectory(KxDynamicStringRefW mountPoint)
	{
		return Dokany2::DokanRemoveMountPoint(mountPoint.data());
	}
	KxDynamicStringRefW& AbstractFS::NormalizeFilePath(KxDynamicStringRefW& path)
	{
		// See if path starts with '\' and remove it. Don't touch '\\?\'.
		auto ExtractPrefix = [](KxDynamicStringRefW& path)
		{
			return path.substr(0, std::size(Utility::LongPathPrefix) - 1);
		};

		if (!path.empty() && ExtractPrefix(path) != Utility::LongPathPrefix)
		{
			size_t count = 0;
			for (const auto& c: path)
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
			path.remove_prefix(count);
		}

		// Remove any trailing '\\'
		size_t count = 0;
		for (auto it = path.rbegin(); it != path.rend(); ++it)
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
		path.remove_suffix(count);
		return path;
	}

	size_t AbstractFS::WriteString(KxDynamicStringRefW source, wchar_t* destination, const size_t maxDstLength)
	{
		const size_t dstBytesLength = maxDstLength * sizeof(wchar_t);
		const size_t srcBytesLength = std::min(dstBytesLength, source.length() * sizeof(wchar_t));

		memcpy_s(destination, dstBytesLength, source.data(), srcBytesLength);
		return srcBytesLength / sizeof(wchar_t);
	}
}

namespace KxVFS
{
	bool AbstractFS::IsWriteRequest(KxDynamicStringRefW filePath, ACCESS_MASK desiredAccess, DWORD createDisposition) const
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

		return (
			desiredAccess & GENERIC_WRITE ||
			createDisposition == CREATE_ALWAYS ||
			(createDisposition == CREATE_NEW && !Utility::IsExist(filePath)) ||
			(createDisposition == OPEN_ALWAYS && !Utility::IsExist(filePath)) ||
			(createDisposition == TRUNCATE_EXISTING && Utility::IsExist(filePath))
			);
	}
	bool AbstractFS::IsDirectory(ULONG kernelCreateOptions) const
	{
		return kernelCreateOptions & FILE_DIRECTORY_FILE;
	}
	
	bool AbstractFS::IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const
	{
		return ((*securityInformation & SACL_SECURITY_INFORMATION) || (*securityInformation & BACKUP_SECURITY_INFORMATION));
	}
	void AbstractFS::ProcessSESecurityPrivilege(PSECURITY_INFORMATION securityInformation) const
	{
		if (!m_Service.HasSeSecurityNamePrivilege())
		{
			*securityInformation &= ~SACL_SECURITY_INFORMATION;
			*securityInformation &= ~BACKUP_SECURITY_INFORMATION;
		}
	}
}

namespace KxVFS
{
	FSError AbstractFS::DoMount()
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
				Utility::CreateFolderTree(m_MountPoint);
			}

			// Allow mount to empty folders
			if (Utility::IsFolderEmpty(m_MountPoint))
			{
				// Update options
				m_Options.ThreadCount = 0;
				m_Options.MountPoint = m_MountPoint.data();
				m_Options.Options = m_Flags;
				m_Options.Timeout = 0;

				// Create file system
				return Dokany2::DokanCreateFileSystem(&m_Options, &m_Operations, &m_Handle);
			}
			return DOKAN_MOUNT_ERROR;
		}
		return DOKAN_ERROR;
	}
	bool AbstractFS::DoUnMount()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA(": ");
		OutputDebugStringA(IsMounted() ? "allowed" : "disallowed");
		OutputDebugStringA("\r\n");

		if (IsMounted())
		{
			Dokany2::DokanCloseHandle(m_Handle);
			return true;
		}
		return false;
	}

	AbstractFS::AbstractFS(Service& service, KxDynamicStringRefW mountPoint, uint32_t flags)
		:m_Service(service), m_MountPoint(mountPoint), m_Flags(flags)
	{
		// Options
		m_Options.GlobalContext = reinterpret_cast<ULONG64>(this);
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
		m_Operations.FindFilesWithPattern = nullptr; // Overriding 'FindFiles' is enough
		m_Operations.FindStreams = Dokan_FindStreams;
	}
	AbstractFS::~AbstractFS()
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		m_IsDestructing = true;
		DoUnMount();
	}

	FSError AbstractFS::Mount()
	{
		return DoMount();
	}
	bool AbstractFS::UnMount()
	{
		return DoUnMount();
	}

	KxDynamicStringW AbstractFS::GetVolumeLabel() const
	{
		return m_Service.GetServiceName();
	}
	KxDynamicStringW AbstractFS::GetVolumeFileSystemName() const
	{
		// File system name could be anything up to 10 characters.
		// But Windows check few feature availability based on file system name.
		// For this, it is recommended to set NTFS or FAT here.
		return L"NTFS";
	}
	uint32_t AbstractFS::GetVolumeSerialNumber() const
	{
		// I assume the volume serial needs to be just a random number,
		// so I think this would suffice. Mirror sample uses '0x19831116' constant.

		#pragma warning (suppress: 4311)
		#pragma warning (suppress: 4302)
		return (uint32_t)(reinterpret_cast<size_t>(this) ^ reinterpret_cast<size_t>(&m_Service));
	}

	Service& AbstractFS::GetService()
	{
		return m_Service;
	}
	const Service& AbstractFS::GetService() const
	{
		return m_Service;
	}

	bool AbstractFS::IsMounted() const
	{
		return m_IsMounted;
	}
	void AbstractFS::SetMounted(bool value)
	{
		m_IsMounted = value;
	}

	KxDynamicStringRefW AbstractFS::GetMountPoint() const
	{
		return m_MountPoint;
	}
	void AbstractFS::SetMountPoint(KxDynamicStringRefW mountPoint)
	{
		SetOptionIfNotMounted(m_MountPoint, mountPoint);
	}

	uint32_t AbstractFS::GetFlags() const
	{
		return m_Flags;
	}
	void AbstractFS::SetFlags(uint32_t flags)
	{
		SetOptionIfNotMounted(m_Flags, flags);
	}

	NTSTATUS AbstractFS::GetNtStatusByWin32ErrorCode(DWORD nWin32ErrorCode) const
	{
		return Dokany2::DokanNtStatusFromWin32(nWin32ErrorCode);
	}
	NTSTATUS AbstractFS::GetNtStatusByWin32LastErrorCode() const
	{
		return Dokany2::DokanNtStatusFromWin32(::GetLastError());
	}

	NTSTATUS AbstractFS::OnMountInternal(EvtMounted& eventInfo)
	{
		m_IsMounted = true;
		m_Service.AddFS(this);

		return OnMount(eventInfo);
	}
	NTSTATUS AbstractFS::OnUnMountInternal(EvtUnMounted& eventInfo)
	{
		OutputDebugStringA(__FUNCTION__);
		OutputDebugStringA("\r\n");

		NTSTATUS statusCode = STATUS_UNSUCCESSFUL;
		if (CriticalSectionLocker lock(m_UnmountCS); true)
		{
			OutputDebugStringA("In EnterCriticalSection: ");
			OutputDebugStringA(__FUNCTION__);
			OutputDebugStringA("\r\n");

			m_IsMounted = false;
			m_Service.RemoveFS(this);

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
	void DOKAN_CALLBACK AbstractFS::Dokan_Mount(EvtMounted* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return (void)GetFromContext(eventInfo->DokanOptions)->OnMountInternal(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_Unmount(EvtUnMounted* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return (void)GetFromContext(eventInfo->DokanOptions)->OnUnMountInternal(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeFreeSpace(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s\r\n"), TEXT(__FUNCTION__));
		return GetFromContext(eventInfo)->OnGetVolumeAttributes(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_CreateFile(EvtCreateFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnCreateFile(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_CloseFile(EvtCloseFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCloseFile(*eventInfo);
	}
	void DOKAN_CALLBACK AbstractFS::Dokan_CleanUp(EvtCleanUp* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return (void)GetFromContext(eventInfo)->OnCleanUp(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_MoveFile(EvtMoveFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\" -> \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, eventInfo->NewFileName);
		return GetFromContext(eventInfo)->OnMoveFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\", DeleteOnClose: %d\r\n"), TEXT(__FUNCTION__), eventInfo->FileName, (int)eventInfo->DokanFileInfo->DeleteOnClose);
		return GetFromContext(eventInfo)->OnCanDeleteFile(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_LockFile(EvtLockFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnLockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_UnlockFile(EvtUnlockFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnUnlockFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileSecurity(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetFileSecurity(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_ReadFile(EvtReadFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnReadFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_WriteFile(EvtWriteFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnWriteFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFlushFileBuffers(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetEndOfFile(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetAllocationSize(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_GetFileInfo(EvtGetFileInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnGetFileInfo(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnSetBasicFileInfo(*eventInfo);
	}

	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FindFiles(EvtFindFiles* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\\*\"\r\n"), TEXT(__FUNCTION__), eventInfo->PathName);
		return GetFromContext(eventInfo)->OnFindFiles(*eventInfo);
	}
	NTSTATUS DOKAN_CALLBACK AbstractFS::Dokan_FindStreams(EvtFindStreams* eventInfo)
	{
		KxVFSDebugPrint(TEXT("%s: \"%s\"\r\n"), TEXT(__FUNCTION__), eventInfo->FileName);
		return GetFromContext(eventInfo)->OnFindStreams(*eventInfo);
	}
}
