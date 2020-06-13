#pragma once

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
