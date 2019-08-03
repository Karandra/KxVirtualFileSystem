/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "ConsoleLogger.h"

namespace KxVFS
{
	size_t ConsoleLogger::LogString(const Logger::InfoPack& infoPack)
	{
		if (HANDLE handle = GetStdHandle(m_StdHandle); handle != INVALID_HANDLE_VALUE)
		{
			ExclusiveSRWLocker lock(m_Lock);

			DWORD charsWritten = 0;
			KxDynamicStringW text = FormatInfoPack(infoPack);
			::WriteConsoleW(handle, text.data(), static_cast<DWORD>(text.size()), &charsWritten, nullptr);
			return static_cast<size_t>(charsWritten);
		}
		return 0;
	}
}
