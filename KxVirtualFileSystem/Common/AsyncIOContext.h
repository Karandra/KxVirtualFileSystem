/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileContext.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
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
