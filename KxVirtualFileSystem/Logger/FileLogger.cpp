/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "FileLogger.h"

namespace KxVFS
{
	size_t FileLogger::LogString(Logger::InfoPack& infoPack)
	{
		if (m_Handle)
		{
			KxDynamicStringA text = FormatInfoPack(infoPack).to_utf8();

			DWORD charsWritten = 0;
			m_Handle.Write(text.data(), static_cast<DWORD>(text.size()), charsWritten);
			return static_cast<size_t>(charsWritten);
		}
		return 0;
	}
}
