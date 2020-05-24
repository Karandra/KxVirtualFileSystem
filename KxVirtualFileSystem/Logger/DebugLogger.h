#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "ILogger.h"

namespace KxVFS
{
	class KxVFS_API DebugLogger: public ILogger
	{
		public:
			size_t LogString(Logger::InfoPack& infoPack) override;
	};
}
