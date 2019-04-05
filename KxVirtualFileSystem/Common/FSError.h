/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/UndefWindows.h"
#include "KxVirtualFileSystem/Utility.h"
#include <optional>

namespace KxVFS
{
	enum class FSErrorCode
	{
		// Special codes
		Success = 0,
		Unknown = -1,

		// Dokany-related
		InternalError,
		BadDriveLetter,
		DriverInstallFailed,
		CanNotStartDriver,
		CanNotMount,
		InvalidMountPoint,
		InvalidVersion,

		// KxVFS codes
		FileContextManagerInitFailed,
		IOManagerInitFialed,
	};
}

namespace KxVFS
{
	class KxVFS_API FSError
	{
		private:
			FSErrorCode m_Code = FSErrorCode::Unknown;

		public:
			FSError() = default;
			FSError(int dokanyErrorCode);
			FSError(FSErrorCode errorCode)
				:m_Code(errorCode)
			{
			}

		public:
			bool IsKnownError() const;
			FSErrorCode GetCode() const;
			std::optional<int> GetDokanyCode() const;
			KxDynamicStringW GetMessage() const;
			
			bool IsSuccess() const
			{
				return m_Code == FSErrorCode::Success;
			}
			bool IsFail() const
			{
				return !IsSuccess();
			}
			bool IsDokanyError() const
			{
				return GetDokanyCode().has_value();
			}
	};
}
