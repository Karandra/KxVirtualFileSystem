/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "DebugLogger.h"

namespace KxVFS
{
	size_t DebugLogger::LogString(Logger::InfoPack& infoPack)
	{
		KxDynamicStringW text = FormatInfoPack(infoPack);
		::OutputDebugStringW(text.data());
		return text.length();
	}
}
