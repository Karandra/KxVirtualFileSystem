#pragma once
#include <utility>
#include <functional>
#include <optional>

namespace KxVFS::Utility
{
	template<class T>
	class CallAtScopeExit
	{
		private:
			std::optional<T> m_Functor;

		public:
			CallAtScopeExit(T&& functor)
				:m_Functor(std::move(functor))
			{
			}
			CallAtScopeExit(const T& functor)
				:m_Functor(functor)
			{
			}
			
			CallAtScopeExit(CallAtScopeExit&& other)
				:m_Functor(std::move(other.m_Functor))
			{
				other.m_Functor.reset();
			}
			CallAtScopeExit(const CallAtScopeExit& other)
				:m_Functor(other.m_Functor)
			{
			}
			
			~CallAtScopeExit()
			{
				if (m_Functor)
				{
					(void)std::invoke(*m_Functor);
				}
			}
			
		public:
			CallAtScopeExit& operator=(CallAtScopeExit&& other)
			{
				m_Functor = std::move(other.m_Functor);
				other.m_Functor.reset();

				return *this;
			}
			CallAtScopeExit& operator=(const CallAtScopeExit& other)
			{
				m_Functor = other.m_Functor;
				return *this;
			}
	};
}
