#include "stdafx.h"
#include "ChainLogger.h"

namespace KxVFS
{
	ChainLogger::~ChainLogger()
	{
		EnumLoggers([](ILogger& logger, bool shouldDelete)
		{
			if (shouldDelete)
			{
				delete &logger;
			}
		});
	}

	size_t ChainLogger::LogString(Logger::InfoPack& infoPack)
	{
		size_t written = 0;
		EnumLoggers([&written, &infoPack](ILogger& logger, bool shouldDelete)
		{
			written += logger.LogString(infoPack);
		});
		return written;
	}
}
