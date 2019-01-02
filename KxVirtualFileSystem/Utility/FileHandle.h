/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class FileHandle final
	{
		private:
			HANDLE m_Handle = INVALID_HANDLE_VALUE;

		public:
			FileHandle(HANDLE fileHandle = INVALID_HANDLE_VALUE)
				:m_Handle(fileHandle)
			{
			}
			FileHandle(FileHandle&& other)
			{
				*this = std::move(other);
			}
			FileHandle(const FileHandle&) = delete;
			~FileHandle()
			{
				Close();
			}
			
			FileHandle& operator=(const HANDLE& fileHandle)
			{
				return Assign(fileHandle);
			}
			FileHandle& operator=(FileHandle&& other)
			{
				Close();
				m_Handle = other.Release();
				return *this;
			}
			FileHandle& operator=(const FileHandle&) = delete;

		public:
			bool IsOK() const
			{
				return m_Handle != INVALID_HANDLE_VALUE;
			}
			HANDLE Get() const
			{
				return m_Handle;
			}
			FileHandle& Assign(const HANDLE& fileHandle)
			{
				Close();
				m_Handle = fileHandle;
				return *this;
			}
		
			HANDLE Release()
			{
				HANDLE fileHandle = m_Handle;
				m_Handle = INVALID_HANDLE_VALUE;
				return fileHandle;
			}
			bool Close()
			{
				if (IsOK())
				{
					return ::CloseHandle(Release());
				}
				return false;
			}
		
			bool SetDeleteOnClose(bool deleteOnClose);
			int64_t GetSize() const
			{
				LARGE_INTEGER size = {0};
				if (::GetFileSizeEx(m_Handle, &size))
				{
					return size.QuadPart;
				}
				return -1;
			}
			bool Seek(int64_t offset, DWORD mode = FILE_CURRENT)
			{
				LARGE_INTEGER pos = {0};
				pos.QuadPart = offset;
				return SetFilePointerEx(m_Handle, pos, &pos, mode);
			}
			int64_t GetPosition() const
			{
				LARGE_INTEGER pos = {0};
				SetFilePointerEx(m_Handle, pos, &pos, FILE_CURRENT);
				return pos.QuadPart;
			}
			KxDynamicStringW GetPath() const;
			uint32_t GetAttributes() const
			{
				BY_HANDLE_FILE_INFORMATION info = {0};
				if (GetFileInformationByHandle(m_Handle, &info))
				{
					return info.dwFileAttributes;
				}
				return INVALID_FILE_ATTRIBUTES;
			}

		public:
			bool operator==(const FileHandle& other) const
			{
				return m_Handle == other.m_Handle;
			}
			bool operator!=(const FileHandle& other) const
			{
				return !(*this == other);
			}

			explicit operator bool() const
			{
				return IsOK();
			}
			bool operator!() const
			{
				return !IsOK();
			}
	
			operator HANDLE() const
			{
				return Get();
			}
	};
}
