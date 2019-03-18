/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ExtendedSecurity.h"
#include <AclAPI.h>

namespace KxVFS
{
	DWORD ExtendedSecurity::GetParentSecurity(KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR* parentSecurity) const
	{
		KxDynamicStringW parentPath = filePath;
		parentPath = parentPath.before_last(L'\\');

		if (!parentPath.empty())
		{
			// Give us everything
			const SECURITY_INFORMATION securityInfo = BACKUP_SECURITY_INFORMATION;

			// Must LocalFree() 'parentSecurity' object
			return ::GetNamedSecurityInfoW(parentPath, SE_FILE_OBJECT, securityInfo, nullptr, nullptr, nullptr, nullptr, parentSecurity);
		}
		return ERROR_PATH_NOT_FOUND;
	}
	DWORD ExtendedSecurity::CreateNewSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const
	{
		if (filePath.empty() || requestedSecurity == nullptr || newSecurity == nullptr)
		{
			return ERROR_INVALID_PARAMETER;
		}

		PSECURITY_DESCRIPTOR parentDescriptor = nullptr;
		KxCallAtScopeExit atExit([&parentDescriptor]()
		{
			::LocalFree(parentDescriptor);
		});

		DWORD errorCode = GetParentSecurity(filePath, &parentDescriptor);
		if (errorCode == ERROR_SUCCESS)
		{
			GENERIC_MAPPING genericMapping =
			{
				FILE_GENERIC_READ,
				FILE_GENERIC_WRITE,
				FILE_GENERIC_EXECUTE,
				FILE_ALL_ACCESS
			};

			GenericHandle accessTokenHandle = Dokany2::DokanOpenRequestorToken(eventInfo.DokanFileInfo);
			if (accessTokenHandle.IsValid() && !accessTokenHandle.IsNull())
			{
				const bool success = ::CreatePrivateObjectSecurity(parentDescriptor,
																   requestedSecurity,
																   newSecurity,
																   eventInfo.DokanFileInfo->IsDirectory,
																   accessTokenHandle,
																   &genericMapping
				);
				if (!success)
				{
					errorCode = ::GetLastError();
				}
			}
			else
			{
				errorCode = ::GetLastError();
			}
		}
		return errorCode;
	}
	SecurityObject ExtendedSecurity::CreateSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, CreationDisposition creationDisposition)
	{
		// We only need security information if there's a possibility a new file could be created
		if (!FileNode::IsRequestToRootNode(filePath) && creationDisposition != CreationDisposition::OpenExisting && creationDisposition != CreationDisposition::TruncateExisting)
		{
			SECURITY_ATTRIBUTES attributes;
			attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
			attributes.lpSecurityDescriptor = eventInfo.SecurityContext.AccessState.SecurityDescriptor;
			attributes.bInheritHandle = FALSE;

			PSECURITY_DESCRIPTOR newFileSecurity = nullptr;
			if (CreateNewSecurity(eventInfo, filePath, eventInfo.SecurityContext.AccessState.SecurityDescriptor, &newFileSecurity) == ERROR_SUCCESS)
			{
				return SecurityObject(newFileSecurity, attributes);
			}
		}
		return nullptr;
	}

	void ExtendedSecurity::OpenWithSecurityAccess(AccessRights& desiredAccess, bool isWriteRequest) const
	{
		desiredAccess |= AccessRights::ReadControl;
		if (isWriteRequest)
		{
			desiredAccess |= AccessRights::WriteDAC;
		}
		if (m_FileSystem.GetService().HasSeSecurityNamePrivilege())
		{
			desiredAccess |= AccessRights::SystemSecurity;
		}
	}
}
