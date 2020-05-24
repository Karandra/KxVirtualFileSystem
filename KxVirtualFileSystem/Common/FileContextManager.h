#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Common/FileContext.h"
#include <atomic>

namespace KxVFS
{
	class IFileSystem;
	class IOManager;
}

namespace KxVFS
{
	class KxVFS_API FileContextManager final
	{
		friend class IOManager;

		private:
			IFileSystem& m_FileSystem;
			IOManager& m_IOManager;	

			std::vector<FileContext*> m_FileContextPool;
			CriticalSection m_FileContextPoolCS;
			size_t m_FileContextPoolMaxSize = 0;

			std::atomic<bool> m_IsUnmounted = false;
			bool m_IsInitialized = false;

		public:
			FileContextManager(FileContextManager&) = delete;
			FileContextManager(IFileSystem& fileSystem) noexcept;

		public:
			IFileSystem& GetFileSystem() noexcept
			{
				return m_FileSystem;
			}
			
			bool IsInitialized() const noexcept
			{
				return m_IsInitialized;
			}
			bool Init();
			void Cleanup() noexcept;

			void PushContext(FileContext& fileContext);
			void DeleteContext(FileContext* fileContext) noexcept;
			FileContext* PopContext(FileHandle fileHandle) noexcept;
	};
}
