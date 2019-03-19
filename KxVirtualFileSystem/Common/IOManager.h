/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Common/FSContext.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API FileContextManager;
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
			bool InitializePendingAsyncIO();

			void CleanupPendingAsyncIO();
			void CleanupAsyncIO();

		private:
			static void CALLBACK AsyncCallback(PTP_CALLBACK_INSTANCE instance,
											   PVOID context,
											   PVOID overlapped,
											   ULONG resultIO,
											   ULONG_PTR bytesTransferred,
											   PTP_IO completionPort
			);

			void OnDeleteFileContext(FileContext& fileContext);
			void OnPushFileContext(FileContext& fileContext);
			bool OnPopFileContext(FileContext& fileContext);

		public:
			IOManager(IOManager&) = delete;
			IOManager(IFileSystem& fileSystem);
			
		public:
			IFileSystem& GetFileSystem()
			{
				return m_FileSystem;
			}
			
			bool IsInitialized() const
			{
				return m_IsInitialized;
			}
			bool Init();
			void Cleanup();

			bool IsAsyncIOEnabled() const
			{
				return m_IsAsyncIOEnabled;
			}
			void EnableAsyncIO(bool enabled = true)
			{
				m_IsAsyncIOEnabled = enabled;
			}
	
		public:
			void DeleteContext(AsyncIOContext* asyncContext);
			void PushContext(AsyncIOContext& asyncContext);
			AsyncIOContext* PopContext(FileContext& fileContext);

		public:
			NTSTATUS ReadFileSync(FileHandle& fileHandle, EvtReadFile& eventInfo, FileContext* fileContext = nullptr) const;
			NTSTATUS WriteFileSync(FileHandle& fileHandle, EvtWriteFile& eventInfo, FileContext* fileContext = nullptr) const;

			NTSTATUS ReadFileAsync(FileContext& fileContext, EvtReadFile& eventInfo);
			NTSTATUS WriteFileAsync(FileContext& fileContext, EvtWriteFile& eventInfo);
	};
}