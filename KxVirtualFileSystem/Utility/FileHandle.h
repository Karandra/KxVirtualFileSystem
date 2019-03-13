/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/HandleWrapper.h"

namespace KxVFS::Utility
{
	enum class KxVFS_API SeekMode
	{
		Current = FILE_CURRENT,
		Start = FILE_BEGIN,
		End = FILE_END,
	};

	class KxVFS_API FileHandle: public HandleWrapper<FileHandle, size_t, std::numeric_limits<size_t>::max()>
	{
		friend class TWrapper;

		protected:
			static void DoCloseHandle(THandle handle)
			{
				::CloseHandle(handle);
			}

		public:
			FileHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}

		public:
			uint32_t GetAttributes() const
			{
				BY_HANDLE_FILE_INFORMATION info = {0};
				if (GetFileInformationByHandle(m_Handle, &info))
				{
					return info.dwFileAttributes;
				}
				return INVALID_FILE_ATTRIBUTES;
			}
			int64_t GetFileSize() const
			{
				LARGE_INTEGER size = {0};
				if (::GetFileSizeEx(m_Handle, &size))
				{
					return size.QuadPart;
				}
				return -1;
			}
			int64_t GetPosition() const
			{
				LARGE_INTEGER pos = {0};
				SetFilePointerEx(m_Handle, pos, &pos, FILE_CURRENT);
				return pos.QuadPart;
			}
			
			bool Seek(int64_t offset, DWORD mode = FILE_CURRENT)
			{
				LARGE_INTEGER pos = {0};
				pos.QuadPart = offset;
				return SetFilePointerEx(m_Handle, pos, &pos, mode);
			}
			bool Seek(int64_t offset, SeekMode mode = SeekMode::Current)
			{
				return Seek(offset, static_cast<DWORD>(mode));
			}
			
			bool SetDeleteOnClose(bool deleteOnClose);
			KxDynamicStringW GetPath() const;
	};
}
