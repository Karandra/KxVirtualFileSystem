#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Utility/Win32Constants.h"
#include "KxVFS/Utility/WinKernelConstants.h"

namespace KxVFS
{
	class KxVFS_API FileContextEventInfo final
	{
		private:
			CreationDisposition m_CreationDisposition = CreationDisposition::None;
			FlagSet<FileAttributes> m_Attributes;
			FlagSet<AccessRights> m_DesiredAccess;
			FlagSet<FileShare> m_ShareMode;
			FlagSet<KernelFileOptions> m_KernelCreationOptions;
			uint32_t m_OriginProcessID = 0;

		public:
			CreationDisposition GetCreationDisposition() const noexcept
			{
				return m_CreationDisposition;
			}
			FlagSet<FileAttributes> GetAttributes() const noexcept
			{
				return m_Attributes;
			}
			FlagSet<AccessRights> GetDesiredAccess() const noexcept
			{
				return m_DesiredAccess;
			}
			FlagSet<FileShare> GetShareMode() const noexcept
			{
				return m_ShareMode;
			}
			uint32_t GetOriginProcessID() const noexcept
			{
				return m_OriginProcessID;
			}

			void Assign(const EvtCreateFile& eventInfo) noexcept;
			void Reset() noexcept
			{
				*this = FileContextEventInfo();
			}
	};
}
