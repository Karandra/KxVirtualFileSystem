#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/IncludeWindows.h"
#include "NtStatusConstants.h"
#include "Win32Constants.h"
#include "GenericHandle.h"

namespace KxVFS
{
	enum class KxVFS_API FileSeekMode: uint32_t
	{
		Current = FILE_CURRENT,
		Start = FILE_BEGIN,
		End = FILE_END,
	};
}

namespace KxVFS
{
	class KxVFS_API FileHandle: public GenericHandle
	{
		friend class TWrapper;

		public:
			FileHandle(THandle fileHandle = GetInvalidHandle()) noexcept
				:GenericHandle(fileHandle)
			{
			}
			FileHandle(DynamicStringRefW path,
					   FlagSet<AccessRights> access,
					   FlagSet<FileShare> share,
					   CreationDisposition disposition,
					   FlagSet<FileAttributes> attributesAndFlags = {},
					   SECURITY_ATTRIBUTES* securityAttributes = nullptr) noexcept
			{
				Create(path, access, share, disposition, attributesAndFlags, securityAttributes);
			}

		public:
			bool Create(DynamicStringRefW path,
						FlagSet<AccessRights> access,
						FlagSet<FileShare> share,
						CreationDisposition disposition,
						FlagSet<FileAttributes> attributesAndFlags = {},
						SECURITY_ATTRIBUTES* securityAttributes = nullptr) noexcept;
			bool OpenVolumeDevice(wchar_t volume, FlagSet<FileAttributes> attributesAndFlags = {}) noexcept;

			FlagSet<FileAttributes> GetAttributes() const noexcept
			{
				BY_HANDLE_FILE_INFORMATION info = {0};
				if (GetInfo(info))
				{
					return FromInt<FileAttributes>(info.dwFileAttributes);
				}
				return FileAttributes::Invalid;
			}
			bool GetFileSize(int64_t& fileSize) const noexcept
			{
				LARGE_INTEGER sizeLI = {};
				if (::GetFileSizeEx(m_Handle, &sizeLI))
				{
					fileSize = sizeLI.QuadPart;
					return true;
				}
				return false;
			}
			int64_t GetFileSize() const noexcept
			{
				int64_t fileSize = -1;
				GetFileSize(fileSize);
				return fileSize;
			}
			
			bool GetInfo(BY_HANDLE_FILE_INFORMATION& fileInfo) const noexcept
			{
				return ::GetFileInformationByHandle(m_Handle, &fileInfo);
			}
			bool SetInfo(_FILE_INFO_BY_HANDLE_CLASS infoClass, const void* fileInfo, size_t size) noexcept
			{
				return ::SetFileInformationByHandle(m_Handle, infoClass, const_cast<void*>(fileInfo), static_cast<DWORD>(size));
			}
			
			template<class TFileInfo>
			bool SetInfo(_FILE_INFO_BY_HANDLE_CLASS infoClass, const TFileInfo& fileInfo) noexcept
			{
				return SetInfo(infoClass, &fileInfo, sizeof(fileInfo));
			}

			bool SetDeleteOnClose(bool deleteOnClose) noexcept;
			DynamicStringW GetPath() const;
			NtStatus SetPath(DynamicStringRefW path, bool replaceIfExist) noexcept;

			int64_t GetPosition() const noexcept
			{
				LARGE_INTEGER pos = {};
				::SetFilePointerEx(m_Handle, pos, &pos, FILE_CURRENT);
				return pos.QuadPart;
			}
			bool Seek(int64_t offset, FileSeekMode mode = FileSeekMode::Current) noexcept
			{
				LARGE_INTEGER pos = {};
				pos.QuadPart = offset;
				return ::SetFilePointerEx(m_Handle, pos, &pos, ToInt(mode));
			}
			
			bool Read(void* buffer, DWORD bytesToRead, DWORD& bytesRead, OVERLAPPED* overlapped = nullptr) noexcept
			{
				return ::ReadFile(m_Handle, buffer, bytesToRead, &bytesRead, overlapped);
			}
			bool Write(const void* buffer, DWORD bytesToWrite, DWORD& bytesWritten, OVERLAPPED* overlapped = nullptr) noexcept
			{
				return ::WriteFile(m_Handle, buffer, bytesToWrite, &bytesWritten, overlapped);
			}
			bool FlushBuffers() noexcept
			{
				return ::FlushFileBuffers(m_Handle);
			}
			bool SetEnd() noexcept
			{
				return ::SetEndOfFile(m_Handle);
			}

			bool Lock(int64_t offset, int64_t length) noexcept;
			bool Unlock(int64_t offset, int64_t length) noexcept;
	};
}
