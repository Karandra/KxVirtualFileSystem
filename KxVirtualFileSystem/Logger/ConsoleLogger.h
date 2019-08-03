/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility/SRWLock.h"
#include "ILogger.h"

namespace KxVFS
{
	class KxVFS_API ConsoleLogger: public ILogger
	{
		public:
			SRWLock m_Lock;
			uint32_t m_StdHandle = 0;

		public:
			ConsoleLogger(uint32_t stdHandle)
				:m_StdHandle(stdHandle)
			{
			}
	
		public:
			size_t LogString(const Logger::InfoPack& infoPack) override;
	};
}

namespace KxVFS
{
	class KxVFS_API StdOutLogger: public ConsoleLogger
	{
		public:
			StdOutLogger()
				:ConsoleLogger(STD_OUTPUT_HANDLE)
			{
			}
	};
	class KxVFS_API StdErrLogger: public ConsoleLogger
	{
		public:
			StdErrLogger()
				:ConsoleLogger(STD_ERROR_HANDLE)
			{
			}
	};
}
