#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/Utility.h"
#include "FileContextEventInfo.h"

namespace KxVFS
{
	class IFileSystem;
	class FileNode;
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
			mutable SRWLock m_Lock;

			PTP_IO m_CompletionPort = nullptr;
			bool m_AsyncIOActive = false;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(IFileSystem& fileSystem) noexcept
				:m_FileSystem(fileSystem)
			{
			}
			~FileContext() noexcept
			{
				CloseHandle();
				CloseThreadpoolIO();
			}

		public:
			[[nodiscard]] MoveableSharedSRWLocker LockShared() noexcept
			{
				return m_Lock;
			}
			[[nodiscard]] MoveableExclusiveSRWLocker LockExclusive() noexcept
			{
				return m_Lock;
			}

			bool IsClosed() const noexcept
			{
				return m_IsClosed;
			}
			void MarkClosed() noexcept
			{
				m_IsClosed = true;
			}
			void MarkOpen() noexcept
			{
				m_IsClosed = false;
			}

			bool IsCleanedUp() const noexcept
			{
				return m_IsCleanedUp;
			}
			void MarkCleanedUp() noexcept
			{
				m_IsCleanedUp = true;
			}
			void MarkActive() noexcept
			{
				m_IsCleanedUp = false;
			}

			void InterlockedGetState(bool& isClosed, bool& isCleanedUp) const noexcept
			{
				if (SharedSRWLocker lock(m_Lock); true)
				{
					isClosed = m_IsClosed;
					isCleanedUp = m_IsCleanedUp;
				}
			}

			IFileSystem& GetFileSystem() const noexcept
			{
				return m_FileSystem;
			}

			FileNode* GetFileNode() const noexcept
			{
				return m_FileNode;
			}
			bool HasFileNode() const noexcept
			{
				return m_FileNode != nullptr;
			}
			void AssignFileNode(FileNode& node) noexcept
			{
				m_FileNode = &node;
			}
			void ResetFileNode() noexcept
			{
				m_FileNode = nullptr;
			}

			const FileHandle& GetHandle() const noexcept
			{
				return m_Handle;
			}
			FileHandle& GetHandle() noexcept
			{
				return m_Handle;
			}
			void AssignHandle(FileHandle handle) noexcept
			{
				m_Handle = std::move(handle);
			}
			void CloseHandle() noexcept
			{
				m_Handle.Close();
			}

			const FileContextEventInfo& GetEventInfo() const noexcept
			{
				return m_EventInfo;
			}
			FileContextEventInfo& GetEventInfo() noexcept
			{
				return m_EventInfo;
			}
			void ResetEventInfo() noexcept
			{
				m_EventInfo.Reset();
			}

			bool IsThreadpoolIOCreated() const noexcept
			{
				return m_CompletionPort != nullptr;
			}
			bool IsThreadpoolIOActive() const noexcept
			{
				return m_AsyncIOActive;
			}
			bool CreateThreadpoolIO(PTP_WIN32_IO_CALLBACK callback, TP_CALLBACK_ENVIRON& environment) noexcept
			{
				if (!m_CompletionPort)
				{
					m_CompletionPort = ::CreateThreadpoolIo(m_Handle, callback, this, &environment);
					return m_CompletionPort != nullptr;
				}
				return false;
			}
			void StartThreadpoolIO() noexcept
			{
				::StartThreadpoolIo(m_CompletionPort);
				m_AsyncIOActive = true;
			}
			void CancelThreadpoolIO() noexcept
			{
				::CancelThreadpoolIo(m_CompletionPort);
				m_AsyncIOActive = false;
			}
			void CloseThreadpoolIO() noexcept
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
