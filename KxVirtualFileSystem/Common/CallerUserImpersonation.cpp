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
			KxVFS_DebugPrint(L"DokanOpenRequestorToken: success");
		}
		else
		{
			// Should we return some error?
			KxVFS_DebugPrint(L"DokanOpenRequestorToken: failed");
		}
		return userTokenHandle;
	}
	bool CallerUserImpersonation::ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const
	{
		if (userTokenHandle)
		{
			if (::ImpersonateLoggedOnUser(userTokenHandle))
			{
				KxVFS_DebugPrint(L"ImpersonateLoggedOnUser: success");
				return true;
			}
			else
			{
				KxVFS_DebugPrint(L"ImpersonateLoggedOnUser: failed with %u error", ::GetLastError());
				return false;
			}
		}
		else
		{
			KxVFS_DebugPrint(L"ImpersonateLoggedOnUser: invalid token");
			return false;
		}
	}
	void CallerUserImpersonation::CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NTSTATUS status) const
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

			::RevertToSelf();
			KxVFS_DebugPrint(L"ImpersonateLoggedOnUser: called 'RevertToSelf' with %u error code", ::GetLastError());

			::SetLastError(lastError);
		}
	}
}
