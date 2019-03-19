/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileContextEventInfo.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API FileNode;
}

namespace KxVFS
{
	class KxVFS_API FileContext final
	{
		private:
			IFileSystem& m_FileSystem;
			FileNode* m_FileNode = nullptr;
			FileHandle m_Handle;
			FileContextEventInfo m_EventInfo;
			mutable CriticalSection m_Lock;

			PTP_IO m_CompletionPort = nullptr;
			bool m_AsyncIOActive = false;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(IFileSystem& fileSystem)
				:m_FileSystem(fileSystem)
			{
			}
			~FileContext()
			{
				CloseHandle();
				CloseThreadpoolIO();
			}

		public:
			[[nodiscard]] MoveableCriticalSectionLocker Lock()
			{
				return m_Lock;
			}
			[[nodiscard]] MoveableCriticalSectionTryLocker TryLock()
			{
				return m_Lock;
			}

			bool IsClosed() const
			{
				return m_IsClosed;
			}
			void MarkClosed()
			{
				m_IsClosed = true;
			}
			void MarkOpen()
			{
				m_IsClosed = false;
			}

			bool IsCleanedUp() const
			{
				return m_IsCleanedUp;
			}
			void MarkCleanedUp()
			{
				m_IsCleanedUp = true;
			}
			void MarkActive()
			{
				m_IsCleanedUp = false;
			}

			void InterlockedGetState(bool& isClosed, bool& isCleanedUp) const
			{
				if (CriticalSectionLocker lock(m_Lock); true)
				{
					isClosed = m_IsClosed;
					isCleanedUp = m_IsCleanedUp;
				}
			}
			std::tuple<bool, bool> InterlockedGetState() const
			{
				bool isCleanedUp = false;
				bool isClosed = false;
				InterlockedGetState(isClosed, isCleanedUp);
				return {isClosed, isCleanedUp};
			}

			IFileSystem& GetFileSystem() const
			{
				return m_FileSystem;
			}

			FileNode* GetFileNode() const
			{
				return m_FileNode;
			}
			bool HasFileNode() const
			{
				return m_FileNode != nullptr;
			}
			void AssignFileNode(FileNode& node)
			{
				m_FileNode = &node;
			}
			void ResetFileNode()
			{
				m_FileNode = nullptr;
			}

			const FileHandle& GetHandle() const
			{
				return m_Handle;
			}
			FileHandle& GetHandle()
			{
				return m_Handle;
			}
			void AssignHandle(FileHandle handle)
			{
				m_Handle = std::move(handle);
			}
			void CloseHandle()
			{
				m_Handle.Close();
			}

			const FileContextEventInfo& GetEventInfo() const
			{
				return m_EventInfo;
			}
			FileContextEventInfo& GetEventInfo()
			{
				return m_EventInfo;
			}
			void ResetEventInfo()
			{
				m_EventInfo.Reset();
			}

			bool IsThreadpoolIOCreated() const
			{
				return m_CompletionPort != nullptr;
			}
			bool IsThreadpoolIOActive() const
			{
				return m_AsyncIOActive;
			}
			bool CreateThreadpoolIO(PTP_WIN32_IO_CALLBACK callback, TP_CALLBACK_ENVIRON& environment)
			{
				if (m_CompletionPort == nullptr)
				{
					m_CompletionPort = ::CreateThreadpoolIo(m_Handle, callback, this, &environment);
					return m_CompletionPort != nullptr;
				}
				return false;
			}
			void StartThreadpoolIO()
			{
				::StartThreadpoolIo(m_CompletionPort);
				m_AsyncIOActive = true;
			}
			void CancelThreadpoolIO()
			{
				::CancelThreadpoolIo(m_CompletionPort);
				m_AsyncIOActive = false;
			}
			void CloseThreadpoolIO()
			{
				if (m_CompletionPort)
				{
					::CloseThreadpoolIo(m_CompletionPort);
					m_CompletionPort = nullptr;
					m_AsyncIOActive = false;
				}
			}
	};
}
