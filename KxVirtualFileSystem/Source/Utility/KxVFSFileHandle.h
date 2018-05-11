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
		KxVFSFileHandle(HANDLE hFileHandle = INVALID_HANDLE_VALUE)
			:m_Handle(hFileHandle)
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
		KxVFSFileHandle& Assign(const HANDLE& hFileHandle)
		{
			Close();
			m_Handle = hFileHandle;
			return *this;
		}
		
		HANDLE Release()
		{
			HANDLE hFileHandle = m_Handle;
			m_Handle = INVALID_HANDLE_VALUE;
			return hFileHandle;
		}
		bool Close()
		{
			if (IsOK())
			{
				return ::CloseHandle(Release());
			}
			return false;
		}
		
		bool SetDeleteOnClose(bool bDeleteOnClose)
		{
			FILE_DISPOSITION_INFO tFileDispositionInfo = {0};
			tFileDispositionInfo.DeleteFile = bDeleteOnClose;

			return ::SetFileInformationByHandle(m_Handle, FileDispositionInfo, &tFileDispositionInfo, sizeof(FILE_DISPOSITION_INFO));
		}
		int64_t GetSize() const
		{
			LARGE_INTEGER tSize = {0};
			if (::GetFileSizeEx(m_Handle, &tSize))
			{
				return tSize.QuadPart;
			}
			return -1;
		}
		bool Seek(int64_t nOffset, DWORD nMode = FILE_CURRENT)
		{
			LARGE_INTEGER tPos = {0};
			tPos.QuadPart = nOffset;
			return SetFilePointerEx(m_Handle, tPos, &tPos, nMode);
		}
		int64_t GetPosition() const
		{
			LARGE_INTEGER tPos = {0};
			SetFilePointerEx(m_Handle, tPos, &tPos, FILE_CURRENT);
			return tPos.QuadPart;
		}
		KxDynamicString GetPath() const;
		DWORD GetAttributes() const
		{
			BY_HANDLE_FILE_INFORMATION tInfo = {0};
			if (GetFileInformationByHandle(m_Handle, &tInfo))
			{
				return tInfo.dwFileAttributes;
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
		
		KxVFSFileHandle& operator=(const HANDLE& hFileHandle)
		{
			return Assign(hFileHandle);
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
