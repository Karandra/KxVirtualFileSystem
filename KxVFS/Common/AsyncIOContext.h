#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/Utility.h"
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
			OVERLAPPED& GetOverlapped() noexcept
			{
				return m_Overlapped;
			}
			void AssignFileContext(FileContext& fileContext) noexcept
			{
				m_FileContext = &fileContext;
			}

		public:
			AsyncIOContext(FileContext& fileContext) noexcept
				:m_FileContext(&fileContext)
			{
			}

		public:
			FileContext& GetFileContext() const noexcept
			{
				return *m_FileContext;
			}
			IFileSystem& GetFileSystem() const noexcept
			{
				return m_FileContext->GetFileSystem();
			}

			int64_t GetOperationOffset() const noexcept
			{
				return Utility::OverlappedOffsetToInt64(m_Overlapped);
			}
			OperationType GetOperationType() const noexcept
			{
				return m_OperationType;
			}
			
			template<class T = void>
			T* GetOperationContext() noexcept
			{
				return reinterpret_cast<T*>(m_OperationContext);
			}
			
			template<class T>
			void SetOperationContext(T& context, int64_t offset, OperationType type) noexcept
			{
				Utility::Int64ToOverlappedOffset(offset, m_Overlapped);
				m_OperationContext = reinterpret_cast<void*>(&context);
				m_OperationType = type;
			}
			
			void ResetOperationContext() noexcept
			{
				Utility::Int64ToOverlappedOffset(0, m_Overlapped);
				m_OperationContext = nullptr;
				m_OperationType = OperationType::Unknown;
			}
	};
}
