/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"

namespace KxVFS
{
	class KxVFS_API Wow64RedirectionDisabler final
	{
		private:
			void* m_Value = nullptr;

		public:
			Wow64RedirectionDisabler()
			{
				::Wow64DisableWow64FsRedirection(&m_Value);
			}
			~Wow64RedirectionDisabler()
			{
				::Wow64RevertWow64FsRedirection(m_Value);
			}
	};
}
