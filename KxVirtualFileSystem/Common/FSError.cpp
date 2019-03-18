/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FSError.h"

namespace
{
	std::optional<int> CheckErrorCode(int code)
	{
		switch (code)
		{
			case DOKAN_SUCCESS:
			case DOKAN_ERROR:
			case DOKAN_DRIVE_LETTER_ERROR:
			case DOKAN_DRIVER_INSTALL_ERROR:
			case DOKAN_START_ERROR:
			case DOKAN_MOUNT_ERROR:
			case DOKAN_MOUNT_POINT_ERROR:
			case DOKAN_VERSION_ERROR:
			{
				return code;
			}
		};
		return std::nullopt;
	}
}

namespace KxVFS
{
	FSError::FSError(int errorCode)
		:m_Code(CheckErrorCode(errorCode))
	{
	}

	bool FSError::IsKnownError() const
	{
		return m_Code.has_value();
	}
	int FSError::GetCode() const
	{
		return IsKnownError() ? *m_Code : -1;
	}

	bool FSError::IsSuccess() const
	{
		return IsKnownError() ? DOKAN_SUCCEEDED(*m_Code) : false;
	}
	bool FSError::IsFail() const
	{
		return !IsKnownError() || DOKAN_FAILED(*m_Code);
	}
	KxDynamicStringW FSError::GetMessage() const
	{
		if (IsKnownError())
		{
			switch (*m_Code)
			{
				case DOKAN_SUCCESS:
				{
					return L"Success";
				}
				case DOKAN_ERROR:
				{
					return L"Mount error";
				}
				case DOKAN_DRIVE_LETTER_ERROR:
				{
					return L"Bad Drive letter";
				}
				case DOKAN_DRIVER_INSTALL_ERROR:
				{
					return L"Can't install driver";
				}
				case DOKAN_START_ERROR:
				{
					return L"Driver answer that something is wrong";
				}
				case DOKAN_MOUNT_ERROR:
				{
					return L"Can't assign a drive letter or mount point, probably already used by another volume";
				}
				case DOKAN_MOUNT_POINT_ERROR:
				{
					return L"Mount point is invalid";
				}
				case DOKAN_VERSION_ERROR:
				{
					return L"Requested an incompatible version";
				}
			};
		}
		return KxDynamicStringW();
	}
}
