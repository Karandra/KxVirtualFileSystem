/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"

namespace KxVFS
{
	class CriticalSection final
	{
		private:
			CRITICAL_SECTION m_CritSec;

		public:
			CriticalSection() noexcept
			{
				::InitializeCriticalSection(&m_CritSec);
			}
			CriticalSection(uint32_t count)
			{
				::InitializeCriticalSectionAndSpinCount(&m_CritSec, count);
			}
			CriticalSection(const CriticalSection&) = delete;
			CriticalSection(CriticalSection&&) = delete;
			~CriticalSection() noexcept
			{
				::DeleteCriticalSection(&m_CritSec);
			}

		public:
			void Enter() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::EnterCriticalSection(&m_CritSec);
				#endif
			}
			bool TryEnter() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				return ::TryEnterCriticalSection(&m_CritSec);
				#else
				return true;
				#endif
			}
			void Leave() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::LeaveCriticalSection(&m_CritSec);
				#endif
			}

		public:
			CriticalSection& operator=(const CriticalSection&) = delete;
			CriticalSection& operator=(CriticalSection&&) = delete;
	};
}

namespace KxVFS::Utility
{
	template<bool t_IsMoveable, bool t_TryLock> class BasicCriticalSectionLocker final
	{
		public:
			constexpr static bool IsMoveable() noexcept
			{
				return t_IsMoveable;
			}

		private:
			CriticalSection* m_CritSec = nullptr;
			bool m_IsTryLocked = false;

		public:
			BasicCriticalSectionLocker(CriticalSection& critSec) noexcept
				:m_CritSec(&critSec)
			{
				if constexpr(t_TryLock)
				{
					m_IsTryLocked = m_CritSec->TryEnter();
				}
				else
				{
					m_CritSec->Enter();
				}
			}
			BasicCriticalSectionLocker(BasicCriticalSectionLocker&& other) noexcept
			{
				*this = std::move(other);
			}
			BasicCriticalSectionLocker(const BasicCriticalSectionLocker&) = delete;
			~BasicCriticalSectionLocker() noexcept
			{
				if constexpr(t_IsMoveable)
				{
					if (m_CritSec == nullptr)
					{
						return;
					}
				}

				if constexpr(t_TryLock)
				{
					if (m_IsTryLocked)
					{
						m_CritSec->Leave();
					}
				}
				else
				{
					m_CritSec->Leave();
				}
			}
			
		public:
			bool IsTryLocked() const noexcept
			{
				return m_IsTryLocked;
			}

		public:
			BasicCriticalSectionLocker& operator=(const BasicCriticalSectionLocker&) = delete;
			BasicCriticalSectionLocker& operator=(BasicCriticalSectionLocker&& other) noexcept
			{
				if constexpr(t_IsMoveable)
				{
					m_CritSec = other.m_CritSec;
					other.m_CritSec = nullptr;
					return *this;
				}
				else
				{
					static_assert(false, "this locker type is not movable");
				}
			}
	};
}

namespace KxVFS
{
	using CriticalSectionLocker = Utility::BasicCriticalSectionLocker<false, false>;
	using MoveableCriticalSectionLocker = Utility::BasicCriticalSectionLocker<true, false>;

	using CriticalSectionTryLocker = Utility::BasicCriticalSectionLocker<false, true>;
	using MoveableCriticalSectionTryLocker = Utility::BasicCriticalSectionLocker<true, true>;
}
