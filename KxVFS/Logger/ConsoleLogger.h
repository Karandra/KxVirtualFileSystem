#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Utility/SRWLock.h"
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
			size_t LogString(Logger::InfoPack& infoPack) override;
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
