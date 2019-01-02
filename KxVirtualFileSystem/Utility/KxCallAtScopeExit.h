/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include <utility>
#include <functional>

namespace KxVFS
{
	template<class T> class KxCallAtScopeExit
	{
		private:
			T m_Functor;

		public:
			KxCallAtScopeExit(const T& functor)
				:m_Functor(functor)
			{
			}
			KxCallAtScopeExit(T&& functor)
				:m_Functor(std::move(functor))
			{
			}
			~KxCallAtScopeExit()
			{
				(void)std::invoke(m_Functor);
			}
	};
}
