/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class [[nodiscard]] SecurityObject
	{
		private:
			SECURITY_ATTRIBUTES m_Attributes = {};
			PSECURITY_DESCRIPTOR m_Descriptor = nullptr;

		public:
			SecurityObject(PSECURITY_DESCRIPTOR value = nullptr, const SECURITY_ATTRIBUTES& attributes = {})
				:m_Descriptor(value), m_Attributes(attributes)
			{
				m_Attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
				m_Attributes.lpSecurityDescriptor = m_Descriptor;
			}
			SecurityObject(SecurityObject&& other)
			{
				*this = std::move(other);
			}
			SecurityObject(const SecurityObject& other) = delete;
			~SecurityObject()
			{
				if (m_Descriptor)
				{
					::DestroyPrivateObjectSecurity(&m_Descriptor);
				}
			}
			
		public:
			const PSECURITY_DESCRIPTOR& GetDescriptor() const
			{
				return m_Descriptor;
			}
			PSECURITY_DESCRIPTOR& GetDescriptor()
			{
				return m_Descriptor;
			}

			const SECURITY_ATTRIBUTES& GetAttributes() const
			{
				return m_Attributes;
			}
			SECURITY_ATTRIBUTES& GetAttributes()
			{
				return m_Attributes;
			}

		public:
			SecurityObject& operator=(SecurityObject&& other)
			{
				Utility::MoveValue(m_Descriptor, other.m_Descriptor);
				Utility::MoveValue(m_Attributes, other.m_Attributes);

				return *this;
			}
			SecurityObject& operator=(const SecurityObject& other) = delete;
	};
}
