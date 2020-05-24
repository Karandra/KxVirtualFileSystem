#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Common/AsyncIOContext.h"

namespace KxVFS
{
	class IFileSystem;
	class FileContextManager;
}

namespace KxVFS
{
	class KxVFS_API IOManager final
	{
		friend class FileContextManager;

		private:
			IFileSystem& m_FileSystem;
			FileContextManager& m_FileContextManager;
			bool m_IsAsyncIOEnabled = false;
			bool m_IsInitialized = false;

			TP_CALLBACK_ENVIRON m_ThreadPoolEnvironment = {0};
			PTP_CLEANUP_GROUP m_ThreadPoolCleanupGroup = nullptr;
			PTP_POOL m_ThreadPool = nullptr;
			CriticalSection m_ThreadPoolCS;

			std::vector<AsyncIOContext*> m_AsyncContextPool;
			CriticalSection m_AsyncContextPoolCS;
			size_t m_AsyncContextPoolMaxSize = 0;

		private:
			bool InitializeAsyncIO();
			bool InitializePendingAsyncIO() noexcept;

			void CleanupPendingAsyncIO() noexcept;
			void CleanupAsyncIO() noexcept;

		private:
			static void CALLBACK AsyncCallback(PTP_CALLBACK_INSTANCE instance,
											   PVOID context,
											   PVOID overlapped,
											   ULONG resultIO,
											   ULONG_PTR bytesTransferred,
											   PTP_IO completionPort
			);

			void OnDeleteFileContext(FileContext& fileContext) noexcept;
			void OnPushFileContext(FileContext& fileContext) noexcept;
			bool OnPopFileContext(FileContext& fileContext);

		public:
			IOManager(IOManager&) = delete;
			IOManager(IFileSystem& fileSystem) noexcept;
			
		public:
			IFileSystem& GetFileSystem() noexcept
			{
				return m_FileSystem;
			}
			
			bool Init();
			void Cleanup() noexcept;
			bool IsInitialized() const noexcept
			{
				return m_IsInitialized;
			}

			bool IsAsyncIOEnabled() const noexcept
			{
				return m_IsAsyncIOEnabled;
			}
			void EnableAsyncIO(bool enabled = true) noexcept
			{
				m_IsAsyncIOEnabled = enabled;
			}
	
		public:
			void DeleteContext(AsyncIOContext* asyncContext) noexcept;
			void PushContext(AsyncIOContext& asyncContext);
			AsyncIOContext* PopContext(FileContext& fileContext) noexcept;

		public:
			NtStatus ReadFileSync(FileHandle& fileHandle, EvtReadFile& eventInfo, FileContext* fileContext = nullptr) const noexcept;
			NtStatus WriteFileSync(FileHandle& fileHandle, EvtWriteFile& eventInfo, FileContext* fileContext = nullptr) const noexcept;

			NtStatus ReadFileAsync(FileContext& fileContext, EvtReadFile& eventInfo) noexcept;
			NtStatus WriteFileAsync(FileContext& fileContext, EvtWriteFile& eventInfo) noexcept;
	};
}
