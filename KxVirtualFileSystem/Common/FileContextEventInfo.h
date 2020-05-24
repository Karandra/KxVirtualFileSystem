#pragma once
#include "KxVirtualFileSystem/Common.hpp"
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
