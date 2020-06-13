#pragma once
#include "KxVFS/Common.hpp"

namespace KxVFS
{
	enum class FSFlags: uint32_t
	{
		None = 0,
		WriteProtected = 1 << 0,
		RemoveableDrive = 1 << 1,
		NetworkDrive = 1 << 2,
		MountManager = 1 << 3,
		FileLockUserMode = 1 << 4,
		AlternateStream = 1 << 5,
		ForceSingleThreaded = 1 << 6,
		CurrentSession = 1 << 7
	};
	KxVFS_DeclareFlagSet(FSFlags);
}
