#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"

namespace KxVFS
{
	class KxVFS_API DisableWOW64FSRedirection final
	{
		private:
			void* m_Value = nullptr;
			bool m_IsSuccess = false;

		public:
			DisableWOW64FSRedirection() noexcept
				:m_IsSuccess(::Wow64DisableWow64FsRedirection(&m_Value))
			{
			}
			DisableWOW64FSRedirection(const DisableWOW64FSRedirection&) = delete;
			DisableWOW64FSRedirection(DisableWOW64FSRedirection&& other) noexcept
			{
				*this = std::move(other);
			}
			~DisableWOW64FSRedirection() noexcept
			{
				if (m_IsSuccess)
				{
					::Wow64RevertWow64FsRedirection(m_Value);
				}
			}

		public:
			explicit operator bool() const noexcept
			{
				return m_IsSuccess;
			}
			bool operator!() const noexcept
			{
				return !m_IsSuccess;
			}

			DisableWOW64FSRedirection& operator=(const DisableWOW64FSRedirection&) noexcept = delete;
			DisableWOW64FSRedirection& operator=(DisableWOW64FSRedirection&& other) noexcept
			{
				m_Value = other.m_Value;
				other.m_Value = nullptr;

				m_IsSuccess = other.m_IsSuccess;
				other.m_IsSuccess = false;

				return *this;
			}
	};
}
