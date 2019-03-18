/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
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
			void CleanupImpersonateCallerUser(TokenHandle& userTokenHandle) const
			{
				// Something which is not 'STATUS_SUCCESS'.
				CleanupImpersonateCallerUser(userTokenHandle, STATUS_NOT_SUPPORTED);
			}
			void CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NTSTATUS status) const;

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
			void CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle) const
			{
				if (m_Enabled)
				{
					return CleanupImpersonateCallerUser(userTokenHandle);
				}
			}
			void CleanupImpersonateCallerUserIfEnabled(TokenHandle& userTokenHandle, NTSTATUS status) const
			{
				if (m_Enabled)
				{
					CleanupImpersonateCallerUser(userTokenHandle, status);
				}
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
