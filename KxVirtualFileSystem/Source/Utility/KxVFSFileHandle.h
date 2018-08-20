/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeDokan.h"
class KxDynamicString;

class KxVFSFileHandle
{
	private:
		HANDLE m_Handle = INVALID_HANDLE_VALUE;

	private:
		bool operator!() const = delete;

	public:
		KxVFSFileHandle(HANDLE fileHandle = INVALID_HANDLE_VALUE)
			:m_Handle(fileHandle)
		{
		}
		~KxVFSFileHandle()
		{
			Close();
		}

	public:
		bool IsOK() const
		{
			return m_Handle != INVALID_HANDLE_VALUE;
		}
		HANDLE Get() const
		{
			return m_Handle;
		}
		KxVFSFileHandle& Assign(const HANDLE& fileHandle)
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
		
		bool SetDeleteOnClose(bool deleteOnClose)
		{
			FILE_DISPOSITION_INFO fileDispositionInfo = {0};
			fileDispositionInfo.DeleteFile = deleteOnClose;

			return ::SetFileInformationByHandle(m_Handle, FileDispositionInfo, &fileDispositionInfo, sizeof(FILE_DISPOSITION_INFO));
		}
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
		KxDynamicString GetPath() const;
		DWORD GetAttributes() const
		{
			BY_HANDLE_FILE_INFORMATION info = {0};
			if (GetFileInformationByHandle(m_Handle, &info))
			{
				return info.dwFileAttributes;
			}
			return INVALID_FILE_ATTRIBUTES;
		}

		operator bool() const
		{
			return IsOK();
		}
		operator HANDLE() const
		{
			return Get();
		}
		
		KxVFSFileHandle& operator=(const HANDLE& fileHandle)
		{
			return Assign(fileHandle);
		}
		bool operator==(const KxVFSFileHandle& other) const
		{
			return m_Handle == other.m_Handle;
		}
		bool operator!=(const KxVFSFileHandle& other) const
		{
			return !(*this == other);
		}
};
