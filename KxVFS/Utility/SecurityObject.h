#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/IncludeWindows.h"
#include "KxVFS/Utility.h"

namespace KxVFS
{
	class [[nodiscard]] SecurityObject
	{
		private:
			SECURITY_ATTRIBUTES m_Attributes = {};
			PSECURITY_DESCRIPTOR m_Descriptor = nullptr;

		public:
			SecurityObject(PSECURITY_DESCRIPTOR value = nullptr, const SECURITY_ATTRIBUTES& attributes = {}) noexcept
				:m_Descriptor(value), m_Attributes(attributes)
			{
				m_Attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
				m_Attributes.lpSecurityDescriptor = m_Descriptor;
			}
			SecurityObject(SecurityObject&& other) noexcept
			{
				*this = std::move(other);
			}
			SecurityObject(const SecurityObject& other) = delete;
			~SecurityObject() noexcept
			{
				if (m_Descriptor)
				{
					::DestroyPrivateObjectSecurity(&m_Descriptor);
				}
			}
			
		public:
			const PSECURITY_DESCRIPTOR& GetDescriptor() const noexcept
			{
				return m_Descriptor;
			}
			PSECURITY_DESCRIPTOR& GetDescriptor() noexcept
			{
				return m_Descriptor;
			}

			const SECURITY_ATTRIBUTES& GetAttributes() const noexcept
			{
				return m_Attributes;
			}
			SECURITY_ATTRIBUTES& GetAttributes() noexcept
			{
				return m_Attributes;
			}

		public:
			SecurityObject& operator=(SecurityObject&& other) noexcept
			{
				Utility::MoveValue(m_Descriptor, other.m_Descriptor);
				Utility::MoveValue(m_Attributes, other.m_Attributes);

				return *this;
			}
			SecurityObject& operator=(const SecurityObject& other) = delete;
	};
}
