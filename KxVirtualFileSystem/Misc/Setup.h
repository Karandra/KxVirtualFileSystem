#pragma once
/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

namespace KxVFS::Setup
{
	namespace Configuration
	{
		#if defined _DEBUG
		constexpr bool Debug = true;
		#else
		constexpr bool Debug = false;
		#endif

		constexpr bool Release = !Debug;
	}

	// Change this to false to globally disable any logging
	constexpr bool EnableLog = true;

	// Change this to true to disable all locks (critical sections and SRW locks)
	constexpr bool DisableLocks = false;
}
