#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "ILogger.h"
#include <vector>

namespace KxVFS
{
	class KxVFS_API ChainLogger: public ILogger
	{
		private:
			std::vector<std::pair<ILogger*, bool>> m_Loggers;

		private:
			template<class TFunc> void EnumLoggers(TFunc&& func)
			{
				for (auto it = m_Loggers.rbegin(); it != m_Loggers.rend(); ++it)
				{
					func(*it->first, it->second);
				}
			}

		public:
			~ChainLogger();

		public:
			size_t LogString(Logger::InfoPack& infoPack) override;

		public:
			ILogger& AddLogger(ILogger& logger)
			{
				m_Loggers.push_back({&logger, false});
				return logger;
			}
			ILogger& AddLogger(std::unique_ptr<ILogger> logger)
			{
				m_Loggers.push_back({logger.get(), true});
				return *logger.release();
			}
	};
}
