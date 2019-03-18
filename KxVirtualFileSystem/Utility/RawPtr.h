/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	template<class TValue> class RawPtr final
	{
		public:
			using pointer = TValue*;
			using element_type = TValue;

		private:
			pointer m_Value = nullptr;

		public:
			RawPtr(pointer ptr = pointer()) noexcept
				:m_Value(ptr)
			{
			}
			RawPtr(RawPtr&& other) noexcept
			{
				*this = std::move(other);
			}
			RawPtr(const RawPtr&) = default;

		public:
			pointer get() const noexcept
			{
				return m_Value;
			}
			void reset(pointer ptr = pointer()) noexcept
			{
				m_Value = ptr;
			}
			void release() noexcept
			{
				m_Value = nullptr;
			}
			void swap(RawPtr& other) noexcept
			{
				std::swap(m_Value, other->m_Value);
			}

		public:
			explicit operator bool() const noexcept
			{
				return m_Value != nullptr;
			}

			operator const pointer() const
			{
				return m_Value;
			}
			operator pointer()
			{
				return m_Value;
			}

			TValue* operator->() const noexcept
			{
				return m_Value;
			}
			TValue& operator*() const
			{
				return *m_Value;
			}

			bool operator==(const RawPtr& other) const noexcept
			{
				return m_Value == other.m_Value;
			}
			bool operator==(std::nullptr_t) const noexcept
			{
				return m_Value == nullptr;
			}
			bool operator!=(const RawPtr& other) const noexcept
			{
				return m_Value != other.m_Value;
			}
			bool operator!=(std::nullptr_t) const noexcept
			{
				return m_Value != nullptr;
			}

			RawPtr& operator=(const RawPtr&) = default;
			RawPtr& operator=(RawPtr&& other) noexcept
			{
				m_Value = other.m_Value;
				other.m_Value = nullptr;

				return *this;
			}
	};
}
