#include "stdafx.h"
#include "KxVFS/Misc/IncludeDokan.h"
#include "KxVFS/FileSystemService.h"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/Utility.h"
#include "CallerUserImpersonation.h"

namespace KxVFS
{
	TokenHandle CallerUserImpersonation::ImpersonateCallerUser(EvtCreateFile& eventInfo) const noexcept
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
	bool CallerUserImpersonation::ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const noexcept
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
	bool CallerUserImpersonation::CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NtStatus status) const noexcept
	{
		if (userTokenHandle)
		{
			// Clean Up operation for impersonate
			const DWORD lastError = ::GetLastError();

			// Keep the handle open for CreateFile
			if (status != NtStatus::Success)
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
