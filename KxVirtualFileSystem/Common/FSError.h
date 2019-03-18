/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/UndefWindows.h"
#include "KxVirtualFileSystem/Utility.h"
#include <optional>

namespace KxVFS
{
	class KxVFS_API FSError
	{
		private:
			std::optional<int> m_Code;

		public:
			FSError() = default;
			FSError(int errorCode);

		public:
			bool IsKnownError() const;
			int GetCode() const;
			
			bool IsSuccess() const;
			bool IsFail() const;
			KxDynamicStringW GetMessage() const;
	};
}
