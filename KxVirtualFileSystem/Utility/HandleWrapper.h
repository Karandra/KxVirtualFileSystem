/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	template<class t_HandleIntType, t_HandleIntType t_InvalidValue> class HandleWrapper
	{
		public:
			using TIntHandle = t_HandleIntType;
			using THandle = HANDLE;

			static constexpr THandle GetInvalidHandle()
			{
				return reinterpret_cast<THandle>(t_InvalidValue);
			}

		protected:
			THandle m_Handle = GetInvalidHandle();

		public:
			HandleWrapper(THandle value = GetInvalidHandle())
				:m_Handle(value)
			{
			}
			HandleWrapper(HandleWrapper&& other)
			{
				*this = std::move(other);
			}
			HandleWrapper(const HandleWrapper&) = delete;
			~HandleWrapper()
			{
				Close();
			}
			
			HandleWrapper& operator=(THandle value)
			{
				return Assign(value);
			}
			HandleWrapper& operator=(HandleWrapper&& other)
			{
				Close();
				m_Handle = other.Release();
				return *this;
			}
			HandleWrapper& operator=(const HandleWrapper&) = delete;

		public:
			bool IsOK() const
			{
				return m_Handle != GetInvalidHandle();
			}
			bool IsNull() const
			{
				return m_Handle != nullptr;
			}
			
			THandle Get() const
			{
				return m_Handle;
			}
			HandleWrapper& Assign(THandle value)
			{
				Close();
				m_Handle = value;
				return *this;
			}
		
			THandle Release()
			{
				THandle value = m_Handle;
				m_Handle = GetInvalidHandle();
				return value;
			}
			bool Close()
			{
				return IsOK() ? ::CloseHandle(Release()) : false;
			}

		public:
			bool operator==(const HandleWrapper& other) const
			{
				return m_Handle == other.m_Handle;
			}
			bool operator!=(const HandleWrapper& other) const
			{
				return !(*this == other);
			}

			explicit operator bool() const
			{
				return IsOK();
			}
			bool operator!() const
			{
				return !IsOK();
			}

			operator THandle() const
			{
				return Get();
			}
	};
}
