/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility/KxDynamicString.h"

class KxVFS_API KxVFSIDispatcher
{
	public:
		virtual KxDynamicString GetTargetPath(const WCHAR* requestedPath) = 0;
};
