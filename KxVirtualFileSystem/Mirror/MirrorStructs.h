/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class MirrorFS;
}

namespace KxVFS::Mirror
{
	class KxVFS_API FSContext
	{
		private:
			MirrorFS* m_FSInstance = nullptr;

		public:
			FSContext(MirrorFS& instance)
				:m_FSInstance(&instance)
			{
			}

		public:
			MirrorFS& GetFSInstance() const
			{
				return *m_FSInstance;
			}
	};
}

namespace KxVFS::Mirror
{
	class FileContext: public FSContext
	{
		public:
			CriticalSection m_Lock;
			HANDLE m_FileHandle = INVALID_HANDLE_VALUE;
			PTP_IO m_IOCompletion = nullptr;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(MirrorFS& instance)
				:FSContext(instance)
			{
			}

	};

	enum class IOOperationType: int
	{
		Unknown = 0,
		Read,
		Write
	};
	class OverlappedContext
	{
		private:
			OVERLAPPED m_InternalOverlapped = {0};

		public:
			FileContext* m_FileContext = nullptr;
			IOOperationType m_IOType = IOOperationType::Unknown;
			void* m_Context = nullptr;

		public:
			OVERLAPPED& GetOverlapped()
			{
				return m_InternalOverlapped;
			}

			MirrorFS& GetFSInstance() const
			{
				return m_FileContext->GetFSInstance();
			}
	};
}
