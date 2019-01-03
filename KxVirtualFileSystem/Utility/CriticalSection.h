/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/IncludeWindows.h"

namespace KxVFS
{
	class CriticalSection final
	{
		private:
			CRITICAL_SECTION m_CritSec;

		public:
			CriticalSection()
			{
				::InitializeCriticalSection(&m_CritSec);
			}
			CriticalSection(uint32_t count)
			{
				::InitializeCriticalSectionAndSpinCount(&m_CritSec, count);
			}
			CriticalSection(const CriticalSection&) = delete;
			~CriticalSection()
			{
				::DeleteCriticalSection(&m_CritSec);
			}

		public:
			void Enter()
			{
				::EnterCriticalSection(&m_CritSec);
			}
			bool TryEnter()
			{
				return ::TryEnterCriticalSection(&m_CritSec);
			}
			void Leave()
			{
				::LeaveCriticalSection(&m_CritSec);
			}
	};
}

namespace KxVFS
{
	class CriticalSectionLocker final
	{
		private:
			CriticalSection& m_CritSec;

		public:
			CriticalSectionLocker(CriticalSection& critSec)
				:m_CritSec(critSec)
			{
				m_CritSec.Enter();
			}
			~CriticalSectionLocker()
			{
				m_CritSec.Leave();
			}
	};
}
