#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
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
	KxVFS_AllowEnumCastOp(FileSeekMode);
}

namespace KxVFS
{
	class KxVFS_API FileHandle: public GenericHandle
	{
		friend class TWrapper;

		public:
			FileHandle(THandle fileHandle = GetInvalidHandle())
				:GenericHandle(fileHandle)
			{
			}
			FileHandle(DynamicStringRefW path,
					   AccessRights access,
					   FileShare share,
					   CreationDisposition disposition,
					   FileAttributes attributesAndFlags = FileAttributes::None,
					   SECURITY_ATTRIBUTES* securityAttributes = nullptr)
			{
				Create(path, access, share, disposition, attributesAndFlags, securityAttributes);
			}

		public:
			bool Create(DynamicStringRefW path,
						AccessRights access,
						FileShare share,
						CreationDisposition disposition,
						FileAttributes attributesAndFlags = FileAttributes::None,
						SECURITY_ATTRIBUTES* securityAttributes = nullptr);
			bool OpenVolumeDevice(wchar_t volume, FileAttributes attributesAndFlags = FileAttributes::None);

			FileAttributes GetAttributes() const
			{
				BY_HANDLE_FILE_INFORMATION info = {0};
				if (GetInfo(info))
				{
					return FromInt<FileAttributes>(info.dwFileAttributes);
				}
				return FileAttributes::Invalid;
			}
			bool GetFileSize(int64_t& fileSize) const
			{
				LARGE_INTEGER sizeLI = {0};
				if (::GetFileSizeEx(m_Handle, &sizeLI))
				{
					fileSize = sizeLI.QuadPart;
					return true;
				}
				return false;
			}
			int64_t GetFileSize() const
			{
				int64_t fileSize = -1;
				GetFileSize(fileSize);
				return fileSize;
			}
			
			bool GetInfo(BY_HANDLE_FILE_INFORMATION& fileInfo) const
			{
				return ::GetFileInformationByHandle(m_Handle, &fileInfo);
			}
			bool SetInfo(_FILE_INFO_BY_HANDLE_CLASS infoClass, const void* fileInfo, size_t size)
			{
				return ::SetFileInformationByHandle(m_Handle, infoClass, const_cast<void*>(fileInfo), static_cast<DWORD>(size));
			}
			template<class TFileInfo> bool SetInfo(_FILE_INFO_BY_HANDLE_CLASS infoClass, const TFileInfo& fileInfo)
			{
				return SetInfo(infoClass, &fileInfo, sizeof(fileInfo));
			}

			bool SetDeleteOnClose(bool deleteOnClose);
			DynamicStringW GetPath() const;
			NtStatus SetPath(DynamicStringRefW path, bool replaceIfExist);

			int64_t GetPosition() const
			{
				LARGE_INTEGER pos = {0};
				::SetFilePointerEx(m_Handle, pos, &pos, FILE_CURRENT);
				return pos.QuadPart;
			}
			bool Seek(int64_t offset, FileSeekMode mode = FileSeekMode::Current)
			{
				LARGE_INTEGER pos = {0};
				pos.QuadPart = offset;
				return ::SetFilePointerEx(m_Handle, pos, &pos, ToInt(mode));
			}
			
			bool Read(void* buffer, DWORD bytesToRead, DWORD& bytesRead, OVERLAPPED* overlapped = nullptr)
			{
				return ::ReadFile(m_Handle, buffer, bytesToRead, &bytesRead, overlapped);
			}
			bool Write(const void* buffer, DWORD bytesToWrite, DWORD& bytesWritten, OVERLAPPED* overlapped = nullptr)
			{
				return ::WriteFile(m_Handle, buffer, bytesToWrite, &bytesWritten, overlapped);
			}
			bool FlushBuffers()
			{
				return ::FlushFileBuffers(m_Handle);
			}
			bool SetEnd()
			{
				return ::SetEndOfFile(m_Handle);
			}

			bool Lock(int64_t offset, int64_t length);
			bool Unlock(int64_t offset, int64_t length);
	};
}
