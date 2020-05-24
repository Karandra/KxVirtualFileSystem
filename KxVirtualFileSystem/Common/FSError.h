#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/UndefWindows.h"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"
#include "KxVirtualFileSystem/Utility.h"
#include <optional>

namespace KxVFS
{
	enum class FSErrorCode
	{
		// Special codes
		Unknown = -1,
		Success = 0,

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
	KxVFS_AllowEnumCastOp(FSErrorCode);
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
			DynamicStringW GetMessage() const;
			
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
