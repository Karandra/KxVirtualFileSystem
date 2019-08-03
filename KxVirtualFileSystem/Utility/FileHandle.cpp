/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileHandle.h"

namespace KxVFS
{
	bool FileHandle::Create(KxDynamicStringRefW path,
							AccessRights access,
							FileShare share,
							CreationDisposition disposition,
							FileAttributes attributesAndFlags,
							SECURITY_ATTRIBUTES* securityAttributes
	)
	{
		Assign(::CreateFileW(path.data(), ToInt(access), ToInt(share), securityAttributes, ToInt(disposition), ToInt(attributesAndFlags), nullptr));
		return IsValid();
	}
	bool FileHandle::OpenVolumeDevice(wchar_t volume, FileAttributes attributesAndFlags)
	{
		return Create(Utility::GetVolumeDevicePath(volume),
					  AccessRights::GenericRead,
					  FileShare::Read|FileShare::Write,
					  CreationDisposition::OpenExisting,
					  attributesAndFlags
		);
	}

	bool FileHandle::SetDeleteOnClose(bool deleteOnClose)
	{
		FILE_DISPOSITION_INFO fileDispositionInfo = {0};
		fileDispositionInfo.DeleteFileW = deleteOnClose;

		return SetInfo(FileDispositionInfo, fileDispositionInfo);
	}
	KxDynamicStringW FileHandle::GetPath() const
	{
		KxDynamicStringW path;
		const DWORD flags = VOLUME_NAME_DOS|FILE_NAME_NORMALIZED;
		const DWORD length = ::GetFinalPathNameByHandleW(m_Handle, nullptr, 0, flags);
		if (length != 0)
		{
			path.resize(length - 1); // -1 for null terminator
			::GetFinalPathNameByHandleW(m_Handle, path.data(), length, flags);

			path.erase(0, 4); // Remove "\\?\" prefix
		}
		return path;
	}
	NTSTATUS FileHandle::SetPath(KxDynamicStringRefW path, bool replaceIfExist)
	{
		// Allocate buffer for rename info
		FILE_RENAME_INFO* renameInfo = nullptr;
		std::vector<uint8_t> renameInfoBuffer;
		try
		{
			// The FILE_RENAME_INFO struct has space for one WCHAR for the name at the end, so that accounts for the null terminator
			const size_t requiredSize = sizeof(FILE_RENAME_INFO) + path.length() * sizeof(path[0]);
			renameInfoBuffer.resize(requiredSize, 0);
			renameInfo = reinterpret_cast<FILE_RENAME_INFO*>(renameInfoBuffer.data());
		}
		catch (std::bad_alloc&)
		{
			return STATUS_BUFFER_OVERFLOW;
		}
		catch (...)
		{
			return STATUS_INTERNAL_ERROR;
		}

		renameInfo->ReplaceIfExists = replaceIfExist;
		renameInfo->RootDirectory = nullptr; // Hope it's never needed, shouldn't be
		renameInfo->FileNameLength = static_cast<DWORD>(path.length() * sizeof(path[0])); // They want length in bytes
		wcscpy_s(renameInfo->FileName, path.length() + 1, path.data());

		if (SetInfo(FileRenameInfo, renameInfo, renameInfoBuffer.size()))
		{
			return STATUS_SUCCESS;
		}
		return IFileSystem::GetNtStatusByWin32LastErrorCode();
	}

	bool FileHandle::Lock(int64_t offset, int64_t length)
	{
		DWORD offsetLowPart = 0;
		DWORD offsetHighPart = 0;
		Utility::Int64ToLowHigh(offset, offsetLowPart, offsetHighPart);

		DWORD lengthLowPart = 0;
		DWORD lengthHighPart = 0;
		Utility::Int64ToLowHigh(length, lengthLowPart, lengthHighPart);

		return ::LockFile(m_Handle, offsetLowPart, offsetHighPart, lengthLowPart, lengthHighPart);
	}
	bool FileHandle::Unlock(int64_t offset, int64_t length)
	{
		DWORD offsetLowPart = 0;
		DWORD offsetHighPart = 0;
		Utility::Int64ToLowHigh(offset, offsetLowPart, offsetHighPart);

		DWORD lengthLowPart = 0;
		DWORD lengthHighPart = 0;
		Utility::Int64ToLowHigh(length, lengthLowPart, lengthHighPart);

		return ::UnlockFile(m_Handle, offsetLowPart, offsetHighPart, lengthLowPart, lengthHighPart);
	}
}
