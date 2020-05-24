#include "stdafx.h"
#include "ConsoleLogger.h"

namespace KxVFS
{
	size_t ConsoleLogger::LogString(Logger::InfoPack& infoPack)
	{
		if (HANDLE handle = GetStdHandle(m_StdHandle); handle != INVALID_HANDLE_VALUE)
		{
			ExclusiveSRWLocker lock(m_Lock);

			DWORD charsWritten = 0;
			DynamicStringW text = FormatInfoPack(infoPack);
			::WriteConsoleW(handle, text.data(), static_cast<DWORD>(text.size()), &charsWritten, nullptr);
			return static_cast<size_t>(charsWritten);
		}
		return 0;
	}
}
