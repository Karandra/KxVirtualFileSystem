/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include <utility>

namespace KxVFS
{
	class SRWLock final
	{
		private:
			SRWLOCK m_Lock = SRWLOCK_INIT;

		public:
			SRWLock() = default;
			SRWLock(const SRWLock&) = delete;
			SRWLock(SRWLock&&) = delete;
			~SRWLock() = default;

		public:
			void AcquireShared() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::AcquireSRWLockShared(&m_Lock);
				#endif
			}
			void AcquireExclusive() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::AcquireSRWLockExclusive(&m_Lock);
				#endif
			}

			bool TryAcquireShared() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				return ::TryAcquireSRWLockShared(&m_Lock);
				#else
				return true;
				#endif
			}
			bool TryAcquireExclusive() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				return ::TryAcquireSRWLockExclusive(&m_Lock);
				#else
				return true;
				#endif
			}

			void ReleaseShared() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::ReleaseSRWLockShared(&m_Lock);
				#endif
			}
			void ReleaseExclusive() noexcept
			{
				#if !KxVFS_DEBUG_DISABLE_LOCKS
				::ReleaseSRWLockExclusive(&m_Lock);
				#endif
			}
	};
}

namespace KxVFS
{
	enum class SRWLockerType
	{
		Shared,
		Exclusive,
	};

	template<SRWLockerType t_LockerType, bool t_IsMoveable> class BasicSRWLocker final
	{
		public:
			constexpr static bool IsShared() noexcept
			{
				return t_LockerType == SRWLockerType::Shared;
			}
			constexpr static bool IsExclusive() noexcept
			{
				return t_LockerType == SRWLockerType::Exclusive;
			}
			constexpr static bool IsMoveable() noexcept
			{
				return t_IsMoveable;
			}

		private:
			SRWLock* m_Lock = nullptr;

		public:
			BasicSRWLocker(SRWLock& lock) noexcept
				:m_Lock(&lock)
			{
				if constexpr(t_LockerType == SRWLockerType::Shared)
				{
					m_Lock->AcquireShared();
				}
				else if constexpr(t_LockerType == SRWLockerType::Exclusive)
				{
					m_Lock->AcquireExclusive();
				}
				else
				{
					static_assert(false, "invalid locker type");
				}
			}
			BasicSRWLocker(BasicSRWLocker&& other) noexcept
			{
				*this = std::move(other);
			}
			BasicSRWLocker(const BasicSRWLocker&) = delete;
			~BasicSRWLocker() noexcept
			{
				if constexpr(t_IsMoveable)
				{
					if (m_Lock == nullptr)
					{
						return;
					}
				}

				if constexpr(t_LockerType == SRWLockerType::Shared)
				{
					m_Lock->ReleaseShared();
				}
				else if constexpr(t_LockerType == SRWLockerType::Exclusive)
				{
					m_Lock->ReleaseExclusive();
				}
				else
				{
					static_assert(false);
				}
			}
			
		public:
			BasicSRWLocker& operator=(const BasicSRWLocker&) = delete;
			BasicSRWLocker& operator=(BasicSRWLocker&& other) noexcept
			{
				if constexpr(t_IsMoveable)
				{
					m_Lock = other.m_Lock;
					other.m_Lock = nullptr;
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
	using SharedSRWLocker = BasicSRWLocker<SRWLockerType::Shared, false>;
	using ExclusiveSRWLocker = BasicSRWLocker<SRWLockerType::Exclusive, false>;

	using MoveableSharedSRWLocker = BasicSRWLocker<SRWLockerType::Shared, true>;
	using MoveableExclusiveSRWLocker = BasicSRWLocker<SRWLockerType::Exclusive, true>;
}
