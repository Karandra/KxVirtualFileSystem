/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
}

namespace KxVFS
{
	class KxVFS_API ExtendedSecurity
	{
		private:
			IFileSystem& m_FileSystem;
			bool m_Enabled = false;

		protected:
			DWORD GetParentSecurity(KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR* parentSecurity) const;
			DWORD CreateNewSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const;
			
			SecurityObject CreateSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, CreationDisposition creationDisposition);
			SecurityObject CreateSecurityIfEnabled(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, CreationDisposition creationDisposition)
			{
				return m_Enabled ? CreateSecurity(eventInfo, filePath, creationDisposition) : nullptr;
			}

			void OpenWithSecurityAccess(AccessRights& desiredAccess, bool isWriteRequest) const;
			void OpenWithSecurityAccessIfEnabled(AccessRights& desiredAccess, bool isWriteRequest) const
			{
				if (m_Enabled)
				{
					OpenWithSecurityAccess(desiredAccess, isWriteRequest);
				}
			}

		public:
			ExtendedSecurity(ExtendedSecurity&) = delete;
			ExtendedSecurity(IFileSystem* fileSystem)
				:m_FileSystem(*fileSystem)
			{
			}

		public:
			bool IsExtendedSecurityEnabled() const
			{
				return m_Enabled;
			}
			void EnableExtendedSecurity(bool enabled = true)
			{
				m_Enabled = enabled;
			}
	};
}
