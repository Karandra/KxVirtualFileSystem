#pragma once
#include "KxVirtualFileSystem/Common.hpp"

namespace KxVFS
{
	class KxVFS_API IRequestDispatcher
	{
		public:
			virtual DynamicStringW DispatchLocationRequest(DynamicStringRefW requestedPath) = 0;
	};
}
