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
	class FileContext
	{
		private:
			MirrorFS* const m_FSInstance = nullptr;
			
		public:
			CriticalSection m_Lock;
			HANDLE m_FileHandle = INVALID_HANDLE_VALUE;
			PTP_IO m_IOCompletion = nullptr;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(MirrorFS* mirror)
				:m_FSInstance(mirror)
			{
			}
	
		public:
			MirrorFS* GetFSInstance() const
			{
				return m_FSInstance;
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
		public:
			OVERLAPPED m_InternalOverlapped = {0};
			FileContext* m_FileContext = nullptr;
			IOOperationType m_IOType = IOOperationType::Unknown;
			void* m_Context = nullptr;
	};
}
