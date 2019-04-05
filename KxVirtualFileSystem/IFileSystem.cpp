/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/Utility.h"
#include "Misc/IncludeDokan.h"
#include "IFileSystem.h"

namespace KxVFS
{
	NTSTATUS IFileSystem::GetNtStatusByWin32ErrorCode(DWORD errorCode)
	{
		return Dokany2::DokanNtStatusFromWin32(errorCode);
	}
	NTSTATUS IFileSystem::GetNtStatusByWin32LastErrorCode()
	{
		return Dokany2::DokanNtStatusFromWin32(::GetLastError());
	}
	std::tuple<FileAttributes, CreationDisposition, AccessRights> IFileSystem::MapKernelToUserCreateFileFlags(const EvtCreateFile& eventInfo)
	{
		DWORD fileAttributesAndFlags = 0;
		DWORD creationDisposition = 0;
		ACCESS_MASK genericDesiredAccess = 0;
		Dokany2::DokanMapKernelToUserCreateFileFlags(const_cast<EvtCreateFile*>(&eventInfo), &genericDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

		return
		{
			FromInt<FileAttributes>(fileAttributesAndFlags),
			FromInt<CreationDisposition>(creationDisposition),
			FromInt<AccessRights>(genericDesiredAccess)
		};
	}

	bool IFileSystem::UnMountDirectory(KxDynamicStringRefW mountPoint)
	{
		return Dokany2::DokanRemoveMountPoint(mountPoint.data());
	}

	uint32_t IFileSystem::ConvertDokanyOptions(FSFlags flags) const
	{
		uint32_t dokanyOptions = 0;
		auto TestAndSet = [&dokanyOptions, flags](uint32_t dokanyOption, FSFlags flag)
		{
			Utility::ModFlagRef(dokanyOptions, dokanyOption, flags & flag);
		};

		TestAndSet(DOKAN_OPTION_DEBUG, FSFlags::Debug);
		TestAndSet(DOKAN_OPTION_STDERR, FSFlags::UseStdErr);
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
	bool IFileSystem::IsWriteRequest(bool isExist, AccessRights desiredAccess, CreationDisposition createDisposition) const
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
			(desiredAccess & AccessRights::GenericWrite) ||
			(createDisposition == CreationDisposition::CreateAlways) ||
			(createDisposition == CreationDisposition::CreateNew && !isExist) ||
			(createDisposition == CreationDisposition::OpenAlways && !isExist) ||
			(createDisposition == CreationDisposition::TruncateExisting && isExist)
			);
	}
	bool IFileSystem::IsWriteRequest(KxDynamicStringRefW filePath, AccessRights desiredAccess, CreationDisposition createDisposition) const
	{
		return IsWriteRequest(Utility::IsAnyExist(filePath), desiredAccess, createDisposition);
	}

	bool IFileSystem::IsRequestingSACLInfo(const PSECURITY_INFORMATION securityInformation) const
	{
		return ((*securityInformation & SACL_SECURITY_INFORMATION) || (*securityInformation & BACKUP_SECURITY_INFORMATION));
	}
	void IFileSystem::ProcessSESecurityPrivilege(PSECURITY_INFORMATION securityInformation) const
	{
		if (!const_cast<IFileSystem*>(this)->GetService().HasSeSecurityNamePrivilege())
		{
			*securityInformation &= ~SACL_SECURITY_INFORMATION;
			*securityInformation &= ~BACKUP_SECURITY_INFORMATION;
		}
	}

	bool IFileSystem::CheckAttributesToOverwriteFile(FileAttributes fileAttributes, FileAttributes requestAttributes, CreationDisposition creationDisposition) const
	{
		const bool fileExist = fileAttributes != FileAttributes::Invalid;
		const bool fileHidden = !(requestAttributes & FileAttributes::Hidden) && fileAttributes & FileAttributes::Hidden;
		const bool fileSystem = !(requestAttributes & FileAttributes::Hidden) && fileAttributes & FileAttributes::System;
		const bool truncateOrCreateAlways = creationDisposition == CreationDisposition::TruncateExisting || creationDisposition == CreationDisposition::CreateAlways;

		if (fileExist && (fileHidden || fileSystem) && truncateOrCreateAlways)
		{
			return false;
		}
		return true;
	}
	bool IFileSystem::IsDirectory(ULONG kernelCreateOptions) const
	{
		return kernelCreateOptions & FILE_DIRECTORY_FILE;
	}
}
