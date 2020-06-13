#pragma once
#include "stdafx.h"
#include "KxVFS/Utility/TokenHandle.h"

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
			TokenHandle ImpersonateCallerUser(EvtCreateFile& eventInfo) const noexcept;
			bool ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const noexcept;
			bool CleanupImpersonateCallerUser(TokenHandle& userTokenHandle) const noexcept
			{
				// Something which is not 'NtStatus::Success'.
				return CleanupImpersonateCallerUser(userTokenHandle, NtStatus::NotSupported);
			}
			bool CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NtStatus status) const noexcept;

			TokenHandle ImpersonateCallerUserIfEnabled(EvtCreateFile& eventInfo) const noexcept
			{
				if (m_Enabled)
				{
					return ImpersonateCallerUser(eventInfo);
				}
				return TokenHandle();
			}
			bool ImpersonateLoggedOnUserIfEnabled(TokenHandle& userTokenHandle) const noexcept
			{
				if (m_Enabled)
				{
					return ImpersonateLoggedOnUser(userTokenHandle);
				}
				return false;
			}
			bool CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle) const noexcept
			{
				if (m_Enabled)
				{
					return CleanupImpersonateCallerUser(userTokenHandle);
				}
				return false;
			}
			bool CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle, NtStatus status) const noexcept
			{
				if (m_Enabled)
				{
					return CleanupImpersonateCallerUser(userTokenHandle, status);
				}
				return false;
			}
			
		public:
			CallerUserImpersonation(CallerUserImpersonation&) = delete;
			CallerUserImpersonation(IFileSystem* fileSystem) noexcept
				:m_FileSystem(*fileSystem)
			{
			}

		public:
			bool ShouldImpersonateCallerUser() const noexcept
			{
				return m_Enabled;
			}
			void EnableImpersonateCallerUser(bool enabled = true) noexcept
			{
				m_Enabled = enabled;
			}
	};
}
