#include "stdafx.h"
#include "FileLogger.h"

namespace KxVFS
{
	size_t FileLogger::LogString(Logger::InfoPack& infoPack)
	{
		if (m_Handle)
		{
			DynamicStringA text = FormatInfoPack(infoPack).to_utf8();

			DWORD charsWritten = 0;
			m_Handle.Write(text.data(), static_cast<DWORD>(text.size()), charsWritten);
			return static_cast<size_t>(charsWritten);
		}
		return 0;
	}
}
