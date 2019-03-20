/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include <utility>
#include <functional>
#include <optional>

namespace KxVFS
{
	template<class T> class KxCallAtScopeExit
	{
		private:
			std::optional<T> m_Functor;

		public:
			KxCallAtScopeExit(T&& functor)
				:m_Functor(std::move(functor))
			{
			}
			KxCallAtScopeExit(const T& functor)
				:m_Functor(functor)
			{
			}
			
			KxCallAtScopeExit(KxCallAtScopeExit&& other)
				:m_Functor(std::move(other.m_Functor))
			{
				other.m_Functor.reset();
			}
			KxCallAtScopeExit(const KxCallAtScopeExit& other)
				:m_Functor(other.m_Functor)
			{
			}
			
			KxCallAtScopeExit& operator=(KxCallAtScopeExit&& other)
			{
				m_Functor = std::move(other.m_Functor);
				other.m_Functor.reset();

				return *this;
			}
			KxCallAtScopeExit& operator=(const KxCallAtScopeExit& other)
			{
				m_Functor = other.m_Functor;
				return *this;
			}

			~KxCallAtScopeExit()
			{
				if (m_Functor)
				{
					(void)std::invoke(*m_Functor);
				}
			}
	};
}
