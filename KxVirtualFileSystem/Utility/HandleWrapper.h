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
			constexpr HandleWrapper(THandle value = GetInvalidHandle()) noexcept
				:m_Handle(value)
			{
			}
			constexpr HandleWrapper(HandleWrapper&& other) noexcept
			{
				*this = std::move(other);
			}
			constexpr HandleWrapper(const HandleWrapper&) = delete;
			~HandleWrapper() noexcept
			{
				Close();
			}

		public:
			constexpr bool IsNull() const noexcept
			{
				return m_Handle == nullptr || m_Handle == GetInvalidHandle();
			}
			constexpr THandle Get() const noexcept
			{
				return m_Handle;
			}
			constexpr HandleWrapper& Assign(THandle value) noexcept
			{
				Close();
				m_Handle = value;
				return *this;
			}
			
			constexpr THandle Release() noexcept
			{
				THandle value = m_Handle;
				m_Handle = GetInvalidHandle();
				return value;
			}
			constexpr bool Close() noexcept
			{
				if (!IsNull())
				{
					TDerived::DoCloseHandle(Release());
					return true;
				}
				return false;
			}

		public:
			constexpr bool operator==(const HandleWrapper& other) const noexcept
			{
				return m_Handle == other.m_Handle;
			}
			constexpr bool operator!=(const HandleWrapper& other) const noexcept
			{
				return !(*this == other);
			}

			constexpr explicit operator bool() const noexcept
			{
				return !IsNull();
			}
			constexpr bool operator!() const noexcept
			{
				return IsNull();
			}
			
			constexpr operator THandle() const noexcept
			{
				return Get();
			}
			constexpr const THandle* operator&() const noexcept
			{
				return &m_Handle;
			}
			constexpr THandle* operator&() noexcept
			{
				return &m_Handle;
			}

			constexpr HandleWrapper& operator=(THandle handle) noexcept
			{
				return Assign(handle);
			}
			constexpr HandleWrapper& operator=(HandleWrapper&& other) noexcept
			{
				return Assign(other.Release());
			}
			constexpr HandleWrapper& operator=(const HandleWrapper&) = delete;
	};
}
