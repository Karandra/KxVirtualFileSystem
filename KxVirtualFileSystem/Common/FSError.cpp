#include "stdafx.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FSError.h"

namespace
{
	KxVFS::FSErrorCode CheckErrorCode(int dokanyErrorCode)
	{
		using KxVFS::FSErrorCode;

		switch (dokanyErrorCode)
		{
			case DOKAN_SUCCESS:
			{
				return FSErrorCode::Success;
			}
			case DOKAN_ERROR:
			{
				return FSErrorCode::Unknown;
			}
			case DOKAN_DRIVE_LETTER_ERROR:
			{
				return FSErrorCode::BadDriveLetter;
			}
			case DOKAN_DRIVER_INSTALL_ERROR:
			{
				return FSErrorCode::DriverInstallFailed;
			}
			case DOKAN_START_ERROR:
			{
				return FSErrorCode::CanNotStartDriver;
			}
			case DOKAN_MOUNT_ERROR:
			{
				return FSErrorCode::CanNotMount;
			}
			case DOKAN_MOUNT_POINT_ERROR:
			{
				return FSErrorCode::InvalidMountPoint;
			}
			case DOKAN_VERSION_ERROR:
			{
				return FSErrorCode::InvalidVersion;
			}
		};
		return FSErrorCode::Unknown;
	}
}

namespace KxVFS
{
	FSError::FSError(int dokanyErrorCode)
		:m_Code(CheckErrorCode(dokanyErrorCode))
	{
	}

	bool FSError::IsKnownError() const
	{
		return m_Code != FSErrorCode::Unknown;
	}
	FSErrorCode FSError::GetCode() const
	{
		return m_Code;
	}
	std::optional<int> FSError::GetDokanyCode() const
	{
		switch (m_Code)
		{
			// Special codes
			case FSErrorCode::Success:
			{
				return DOKAN_SUCCESS;
			}
			case FSErrorCode::Unknown:
			{
				return DOKAN_ERROR;
			}

			// Dokany-related
			case FSErrorCode::BadDriveLetter:
			{
				return DOKAN_DRIVE_LETTER_ERROR;
			}
			case FSErrorCode::DriverInstallFailed:
			{
				return DOKAN_DRIVER_INSTALL_ERROR;
			}
			case FSErrorCode::CanNotStartDriver:
			{
				return DOKAN_START_ERROR;
			}
			case FSErrorCode::CanNotMount:
			{
				return DOKAN_MOUNT_ERROR;
			}
			case FSErrorCode::InvalidMountPoint:
			{
				return DOKAN_MOUNT_POINT_ERROR;
			}
			case FSErrorCode::InvalidVersion:
			{
				return DOKAN_VERSION_ERROR;
			}
		};
		return std::nullopt;
	}
	DynamicStringW FSError::GetMessage() const
	{
		switch (m_Code)
		{
			// Special codes
			case FSErrorCode::Success:
			{
				return L"Success";
			}
			case FSErrorCode::Unknown:
			{
				return L"Unknown error";
			}

			// Dokany-related
			case FSErrorCode::BadDriveLetter:
			{
				return L"Bad drive letter";
			}
			case FSErrorCode::DriverInstallFailed:
			{
				return L"Can't install driver";
			}
			case FSErrorCode::CanNotStartDriver:
			{
				return L"Driver answer that something is wrong";
			}
			case FSErrorCode::CanNotMount:
			{
				return L"Can't assign a drive letter or mount point, probably already used by another volume or this file system instance is already mounted";
			}
			case FSErrorCode::InvalidMountPoint:
			{
				return L"Mount point is invalid";
			}
			case FSErrorCode::InvalidVersion:
			{
				return L"Requested an incompatible version";
			}

			// KxVFS codes
			case FSErrorCode::FileContextManagerInitFailed:
			{
				return L"Failed to initialize file context manager";
			}
			case FSErrorCode::IOManagerInitFialed:
			{
				return L"Failed to initialize IO manager";
			}
		};
		return DynamicStringW();
	}
}
