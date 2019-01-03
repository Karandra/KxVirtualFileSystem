/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility
{
	class [[nodiscard]] SecurityObject
	{
		private:
			PSECURITY_DESCRIPTOR m_Descriptor = nullptr;

		public:
			SecurityObject(PSECURITY_DESCRIPTOR value = nullptr)
				:m_Descriptor(value)
			{
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
			SecurityObject& operator=(SecurityObject&& other)
			{
				m_Descriptor = other.m_Descriptor;
				other.m_Descriptor = nullptr;
			}
			SecurityObject& operator=(const SecurityObject& other) = delete;
	};
}
