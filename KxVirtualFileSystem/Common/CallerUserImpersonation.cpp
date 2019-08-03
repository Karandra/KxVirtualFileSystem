/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "CallerUserImpersonation.h"

namespace KxVFS
{
	TokenHandle CallerUserImpersonation::ImpersonateCallerUser(EvtCreateFile& eventInfo) const
	{
		TokenHandle userTokenHandle = Dokany2::DokanOpenRequestorToken(eventInfo.DokanFileInfo);
		if (userTokenHandle)
		{
			KxVFS_Log(LogLevel::Info, L"DokanOpenRequestorToken: success");
		}
		else
		{
			// Should we return some error?
			KxVFS_Log(LogLevel::Info, L"DokanOpenRequestorToken: failed");
		}
		return userTokenHandle;
	}
	bool CallerUserImpersonation::ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const
	{
		if (userTokenHandle)
		{
			if (::ImpersonateLoggedOnUser(userTokenHandle))
			{
				KxVFS_Log(LogLevel::Info, L"ImpersonateLoggedOnUser: success");
				return true;
			}
			else
			{
				KxVFS_Log(LogLevel::Info, L"ImpersonateLoggedOnUser: failed with %1 error", ::GetLastError());
				return false;
			}
		}
		else
		{
			KxVFS_Log(LogLevel::Info, L"ImpersonateLoggedOnUser: invalid token");
			return false;
		}
	}
	bool CallerUserImpersonation::CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NTSTATUS status) const
	{
		if (userTokenHandle)
		{
			// Clean Up operation for impersonate
			const DWORD lastError = ::GetLastError();

			// Keep the handle open for CreateFile
			if (status != STATUS_SUCCESS)
			{
				userTokenHandle.Close();
			}

			const bool success = ::RevertToSelf();
			KxVFS_Log(LogLevel::Info, L"ImpersonateLoggedOnUser: called 'RevertToSelf' with %1 error code", ::GetLastError());
			::SetLastError(lastError);
			return success;
		}
		return false;
	}
}
