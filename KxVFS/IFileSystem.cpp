#include "stdafx.h"
#include "KxVFS/FileSystemService.h"
#include "KxVFS/Utility.h"
#include "Misc/IncludeDokan.h"
#include "IFileSystem.h"

namespace KxVFS
{
	NtStatus IFileSystem::GetNtStatusByWin32ErrorCode(DWORD errorCode)
	{
		return FromInt<NtStatus>(Dokany2::DokanNtStatusFromWin32(errorCode));
	}
	NtStatus IFileSystem::GetNtStatusByWin32LastErrorCode()
	{
		return FromInt<NtStatus>(Dokany2::DokanNtStatusFromWin32(::GetLastError()));
	}
	std::tuple<FlagSet<FileAttributes>, CreationDisposition, FlagSet<AccessRights>> IFileSystem::MapKernelToUserCreateFileFlags(const EvtCreateFile& eventInfo)
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

	bool IFileSystem::UnMountDirectory(DynamicStringRefW mountPoint)
	{
		return Dokany2::DokanRemoveMountPoint(mountPoint.data());
	}

	bool IFileSystem::IsWriteRequest(bool isExist, FlagSet<AccessRights> desiredAccess, CreationDisposition createDisposition) const
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
	bool IFileSystem::IsWriteRequest(DynamicStringRefW filePath, FlagSet<AccessRights> desiredAccess, CreationDisposition createDisposition) const
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

	bool IFileSystem::CheckAttributesToOverwriteFile(FlagSet<FileAttributes> fileAttributes, FlagSet<FileAttributes> requestAttributes, CreationDisposition creationDisposition) const
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
