/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
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
