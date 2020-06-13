#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/UndefWindows.h"
#include "KxVFS/Utility.h"
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
}

namespace KxVFS
{
	class KxVFS_API FSError final
	{
		private:
			FSErrorCode m_Code = FSErrorCode::Unknown;

		public:
			FSError() noexcept = default;
			FSError(int dokanyErrorCode) noexcept;
			FSError(FSErrorCode errorCode) noexcept
				:m_Code(errorCode)
			{
			}

		public:
			bool IsKnownError() const noexcept;
			FSErrorCode GetCode() const noexcept;
			std::optional<int> GetDokanyCode() const noexcept;
			DynamicStringW GetMessage() const;
			
			bool IsSuccess() const noexcept
			{
				return m_Code == FSErrorCode::Success;
			}
			bool IsFail() const noexcept
			{
				return !IsSuccess();
			}
			bool IsDokanyError() const noexcept
			{
				return GetDokanyCode().has_value();
			}
	};
}
