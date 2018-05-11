#pragma once
#include "KxVFSIncludeDokan.h"

class KxVFSCriticalSection
{
	private:
		CRITICAL_SECTION m_CritSec;

	public:
		KxVFSCriticalSection()
		{
			::InitializeCriticalSection(&m_CritSec);
		}
		KxVFSCriticalSection(DWORD nCount)
		{
			::InitializeCriticalSectionAndSpinCount(&m_CritSec, nCount);
		}
		~KxVFSCriticalSection()
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

class KxVFSCriticalSectionLocker
{
	private:
		KxVFSCriticalSection& m_CritSec;
		bool m_ShouldLeave = true;

	public:
		KxVFSCriticalSectionLocker(KxVFSCriticalSection& critSec)
			:m_CritSec(critSec)
		{
			m_CritSec.Enter();
		}
		~KxVFSCriticalSectionLocker()
		{
			if (m_ShouldLeave)
			{
				m_CritSec.Leave();
			}
		}

	public:
		const KxVFSCriticalSection& GetCritSection() const
		{
			return m_CritSec;
		}
		KxVFSCriticalSection& GetCritSection()
		{
			return m_CritSec;
		}

		void Leave()
		{
			if (m_ShouldLeave)
			{
				m_CritSec.Leave();
				m_ShouldLeave = false;
			}
		}
};
