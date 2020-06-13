#include "stdafx.h"
#include "DebugLogger.h"

namespace KxVFS
{
	size_t DebugLogger::LogString(Logger::InfoPack& infoPack)
	{
		DynamicStringW text = FormatInfoPack(infoPack);
		::OutputDebugStringW(text.data());
		return text.length();
	}
}
