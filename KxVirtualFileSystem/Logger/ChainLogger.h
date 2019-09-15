/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "ILogger.h"

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
