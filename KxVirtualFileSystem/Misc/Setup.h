#pragma once
/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

namespace KxVFS::Setup
{
	// Change this to true to enable debug info printing to console and debugger output window
	#if defined _DEBUG
	constexpr bool EnableDebugLog = true;
	#else
	constexpr bool EnableDebugLog = false;
	#endif

	// Change this to true to disable all locks (critical sections and SRW locks)
	constexpr bool DisableLocks = false;
}
