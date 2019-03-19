/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility/Win32Constants.h"
#include "KxVirtualFileSystem/Utility/WinKernelConstants.h"

namespace KxVFS
{
	class KxVFS_API FileContextEventInfo
	{
		private:
			CreationDisposition m_CreationDisposition = CreationDisposition::None;
			FileAttributes m_Attributes = FileAttributes::None;
			AccessRights m_DesiredAccess = AccessRights::None;
			FileShare m_ShareMode = FileShare::None;
			KernelFileOptions m_KernelCreationOptions = KernelFileOptions::None;
			uint32_t m_OriginProcessID = 0;

		public:
			CreationDisposition GetCreationDisposition() const
			{
				return m_CreationDisposition;
			}
			FileAttributes GetAttributes() const
			{
				return m_Attributes;
			}
			AccessRights GetDesiredAccess() const
			{
				return m_DesiredAccess;
			}
			FileShare GetShareMode() const
			{
				return m_ShareMode;
			}
			uint32_t GetOriginProcessID() const
			{
				return m_OriginProcessID;
			}

			void Assign(const EvtCreateFile& eventInfo);
			void Reset()
			{
				*this = FileContextEventInfo();
			}
	};
}
