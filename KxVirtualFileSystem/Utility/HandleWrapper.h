#pragma once
#include "KxVirtualFileSystem/Common.hpp"

namespace KxVFS
{
	template<class t_Derived, class t_HandleType, class t_HandleIntType, t_HandleIntType t_InvalidValue>
	class HandleWrapper
	{
		public:
			using TWrapper = HandleWrapper;
			using TDerived = t_Derived;

			using THandle = t_HandleType;
			using TIntHandle = t_HandleIntType;

			static constexpr THandle GetInvalidHandle() noexcept
			{
				return reinterpret_cast<THandle>(t_InvalidValue);
			}

		protected:
			THandle m_Handle = GetInvalidHandle();

		public:
			HandleWrapper(THandle value = GetInvalidHandle()) noexcept
				:m_Handle(value)
			{
			}
			HandleWrapper(HandleWrapper&& other) noexcept
			{
				*this = std::move(other);
			}
			HandleWrapper(const HandleWrapper&) = delete;
			~HandleWrapper() noexcept
			{
				Close();
			}
			
			HandleWrapper& operator=(THandle handle) noexcept
			{
				return Assign(handle);
			}
			HandleWrapper& operator=(HandleWrapper&& other) noexcept
			{
				return Assign(other.Release());
			}
			HandleWrapper& operator=(const HandleWrapper&) = delete;

		public:
			bool IsValidNonNull() const noexcept
			{
				return IsValid() && !IsNull();
			}
			bool IsValid() const noexcept
			{
				return m_Handle != GetInvalidHandle();
			}
			bool IsNull() const noexcept
			{
				return m_Handle == nullptr;
			}
			
			THandle Get() const noexcept
			{
				return m_Handle;
			}
			HandleWrapper& Assign(THandle value) noexcept
			{
				Close();
				m_Handle = value;
				return *this;
			}
			
			THandle Release() noexcept
			{
				THandle value = m_Handle;
				m_Handle = GetInvalidHandle();
				return value;
			}
			bool Close() noexcept
			{
				if (IsValid())
				{
					TDerived::DoCloseHandle(Release());
					return true;
				}
				return false;
			}

		public:
			bool operator==(const HandleWrapper& other) const noexcept
			{
				return m_Handle == other.m_Handle;
			}
			bool operator!=(const HandleWrapper& other) const noexcept
			{
				return !(*this == other);
			}

			explicit operator bool() const noexcept
			{
				return IsValid();
			}
			bool operator!() const noexcept
			{
				return !IsValid();
			}
			operator THandle() const noexcept
			{
				return Get();
			}
			
			const THandle* operator&() const noexcept
			{
				return &m_Handle;
			}
			THandle* operator&() noexcept
			{
				return &m_Handle;
			}
	};
}
