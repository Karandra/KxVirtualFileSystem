#pragma once
#include "KxVirtualFileSystem/Common.hpp"

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
			DWORD GetParentSecurity(DynamicStringRefW filePath, PSECURITY_DESCRIPTOR* parentSecurity) const;
			DWORD CreateNewSecurity(EvtCreateFile& eventInfo, DynamicStringRefW filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const;
			
			SecurityObject CreateSecurity(EvtCreateFile& eventInfo, DynamicStringRefW filePath, CreationDisposition creationDisposition);
			SecurityObject CreateSecurityIfEnabled(EvtCreateFile& eventInfo, DynamicStringRefW filePath, CreationDisposition creationDisposition)
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
