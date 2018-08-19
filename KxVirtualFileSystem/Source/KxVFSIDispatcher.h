#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility/KxDynamicString.h"

class KxVFS_API KxVFSIDispatcher
{
	public:
		virtual KxDynamicString GetTargetPath(const WCHAR* requestedPath) = 0;
};
