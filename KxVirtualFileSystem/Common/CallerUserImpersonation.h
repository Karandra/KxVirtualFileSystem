#pragma once
#include "stdafx.h"
#include "KxVirtualFileSystem/Utility/TokenHandle.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
}

namespace KxVFS
{
	class KxVFS_API CallerUserImpersonation
	{
		private:
			IFileSystem& m_FileSystem;
			bool m_Enabled = false;

		protected:
			TokenHandle ImpersonateCallerUser(EvtCreateFile& eventInfo) const;
			bool ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const;
			bool CleanupImpersonateCallerUser(TokenHandle& userTokenHandle) const
			{
				// Something which is not 'NtStatus::Success'.
				return CleanupImpersonateCallerUser(userTokenHandle, NtStatus::NotSupported);
			}
			bool CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NtStatus status) const;

			TokenHandle ImpersonateCallerUserIfEnabled(EvtCreateFile& eventInfo) const
			{
				if (m_Enabled)
				{
					return ImpersonateCallerUser(eventInfo);
				}
				return TokenHandle();
			}
			bool ImpersonateLoggedOnUserIfEnabled(TokenHandle& userTokenHandle) const
			{
				if (m_Enabled)
				{
					return ImpersonateLoggedOnUser(userTokenHandle);
				}
				return false;
			}
			bool CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle) const
			{
				if (m_Enabled)
				{
					return CleanupImpersonateCallerUser(userTokenHandle);
				}
				return false;
			}
			bool CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle, NtStatus status) const
			{
				if (m_Enabled)
				{
					return CleanupImpersonateCallerUser(userTokenHandle, status);
				}
				return false;
			}
			
		public:
			CallerUserImpersonation(CallerUserImpersonation&) = delete;
			CallerUserImpersonation(IFileSystem* fileSystem)
				:m_FileSystem(*fileSystem)
			{
			}

		public:
			bool ShouldImpersonateCallerUser() const
			{
				return m_Enabled;
			}
			void EnableImpersonateCallerUser(bool enabled = true)
			{
				m_Enabled = enabled;
			}
	};
}
