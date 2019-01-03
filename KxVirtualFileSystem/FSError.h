/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include <optional>
#include "UndefWindows.h"

namespace KxVFS
{
	class FSError
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
