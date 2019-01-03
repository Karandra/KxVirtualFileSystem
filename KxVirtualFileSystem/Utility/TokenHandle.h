/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	using TokenHandle = HandleWrapper<size_t, std::numeric_limits<size_t>::max()>;
}
