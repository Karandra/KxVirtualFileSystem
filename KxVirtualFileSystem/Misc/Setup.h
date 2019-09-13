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
		#if defined KxVFS_CONFIGURATION_RELEASE && !defined _DEBUG
		constexpr bool Release = true;
		#else
		constexpr bool Release = false;
		#endif

		#if defined KxVFS_CONFIGURATION_RELWITHLOG && !defined _DEBUG
		constexpr bool RelWithLog = true;
		#else
		constexpr bool RelWithLog = false;
		#endif

		#if defined KxVFS_CONFIGURATION_DEBUG || defined _DEBUG
		constexpr bool Debug = true;
		#else
		constexpr bool Debug = false;
		#endif
	}

	// Change this to true to enable debug info printing
	constexpr bool EnableLog = Configuration::Debug || Configuration::RelWithLog;

	// Change this to true to disable all locks (critical sections and SRW locks)
	constexpr bool DisableLocks = false;
}
