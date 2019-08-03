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
	class KxVFS_API DebugLogger: public ILogger
	{
		public:
			size_t LogString(const Logger::InfoPack& infoPack) override;
	};
}
