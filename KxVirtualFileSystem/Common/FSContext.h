/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API IOManager;
	class KxVFS_API FileNode;
}

namespace KxVFS
{
	class KxVFS_API FSContext
	{
		private:
			IFileSystem& m_FileSystem;
			FileNode* m_FileNode = nullptr;

		public:
			FSContext(IFileSystem& fileSystem)
				:m_FileSystem(fileSystem)
			{
			}
			virtual ~FSContext() = default;

		public:
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
	};
}

namespace KxVFS
{
	class KxVFS_API FileOptionsStore
	{
		private:
			CreationDisposition m_CreationDisposition = CreationDisposition::None;
			FileAttributes m_Attributes = FileAttributes::None;
			AccessRights m_DesiredAccess = AccessRights::None;
			FileShare m_ShareMode = FileShare::None;
			KernelFileOptions m_KernelCreationOptions = KernelFileOptions::None;

		public:
			CreationDisposition GetCreationDisposition() const
			{
				return m_CreationDisposition;
			}
			FileAttributes GetAttributes() const
			{
				return m_Attributes;
			}
			AccessRights GetDesiredAccess() const
			{
				return m_DesiredAccess;
			}
			FileShare GetShareMode() const
			{
				return m_ShareMode;
			}
			
			void Assign(CreationDisposition disposition, FileAttributes attributes, AccessRights access, FileShare shareMode, KernelFileOptions kernelOptions)
			{
				m_CreationDisposition = disposition;
				m_Attributes = attributes;
				m_DesiredAccess = access;
				m_ShareMode = shareMode;
				m_KernelCreationOptions = kernelOptions;
			}
			void Assign(const EvtCreateFile& eventInfo);
			void Reset()
			{
				*this = FileOptionsStore();
			}
	};
}

namespace KxVFS
{
	class KxVFS_API FileContext final: public FSContext
	{
		private:
			mutable CriticalSection m_Lock;
			FileHandle m_Handle;
			FileOptionsStore m_Options;

			PTP_IO m_CompletionPort = nullptr;
			bool m_AsyncIOActive = false;

			bool m_IsCleanedUp = false;
			bool m_IsClosed = false;

		public:
			FileContext(IFileSystem& fileSystem)
				:FSContext(fileSystem)
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
			
			const FileOptionsStore& GetOptions() const
			{
				return m_Options;
			}
			FileOptionsStore& GetOptions()
			{
				return m_Options;
			}
			void ResetOptions()
			{
				m_Options.Reset();
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

namespace KxVFS
{
	class KxVFS_API AsyncIOContext final
	{
		friend class IOManager;

		public:
			enum class OperationType: int32_t
			{
				Unknown = 0,
				Read,
				Write
			};

		private:
			OVERLAPPED m_Overlapped = {0};
			FileContext* m_FileContext = nullptr;
			void* m_OperationContext = nullptr;
			OperationType m_OperationType = OperationType::Unknown;

		private:
			OVERLAPPED& GetOverlapped()
			{
				return m_Overlapped;
			}
			void AssignFileContext(FileContext& fileContext)
			{
				m_FileContext = &fileContext;
			}

		public:
			AsyncIOContext(FileContext& fileContext)
				:m_FileContext(&fileContext)
			{
			}

		public:
			FileContext& GetFileContext() const
			{
				return *m_FileContext;
			}
			IFileSystem& GetFileSystem() const
			{
				return m_FileContext->GetFileSystem();
			}

			int64_t GetOperationOffset() const
			{
				int64_t offset = 0;
				Utility::OverlappedOffsetToInt64(offset, m_Overlapped);
				return offset;
			}
			OperationType GetOperationType() const
			{
				return m_OperationType;
			}
			
			template<class T = void> T* GetOperationContext()
			{
				return reinterpret_cast<T*>(m_OperationContext);
			}
			template<class T> void SetOperationContext(T& context, int64_t offset, OperationType type)
			{
				Utility::Int64ToOverlappedOffset(offset, m_Overlapped);
				m_OperationContext = reinterpret_cast<void*>(&context);
				m_OperationType = type;
			}
			void ResetOperationContext()
			{
				Utility::Int64ToOverlappedOffset(0i64, m_Overlapped);
				m_OperationContext = nullptr;
				m_OperationType = OperationType::Unknown;
			}
	};
}
