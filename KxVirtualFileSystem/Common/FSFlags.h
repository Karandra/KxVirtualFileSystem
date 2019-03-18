/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"

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
		CurrentSession = 1 << 7,
		UseStdErr = 1 << 8,
		Debug = 1 << 9,
	};
	KxVFS_AllowEnumBitwiseOp(FSFlags);
}
