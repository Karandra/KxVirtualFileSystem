/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "FileHandle.h"

namespace KxVFS
{
	KxDynamicStringW FileHandle::GetPath() const
	{
		KxDynamicStringW out;
		DWORD flags = VOLUME_NAME_DOS|FILE_NAME_NORMALIZED;
		DWORD length = ::GetFinalPathNameByHandleW(m_Handle, nullptr, 0, flags);
		if (length != 0)
		{
			out.reserve(length);
			::GetFinalPathNameByHandleW(m_Handle, out.data(), static_cast<DWORD>(out.length()), flags);
			out.erase(0, 4); // Remove "\\?\" prefix
		}
		return out;
	}
}
