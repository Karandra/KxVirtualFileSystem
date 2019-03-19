/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Common/FileContext.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API IOManager;
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

			volatile LONG m_IsUnmounted = 0;
			bool m_IsInitialized = false;

		public:
			FileContextManager(FileContextManager&) = delete;
			FileContextManager(IFileSystem& fileSystem);

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

			void DeleteContext(FileContext* fileContext);
			void PushContext(FileContext& fileContext);
			FileContext* PopContext(FileHandle fileHandle);
	};
}
