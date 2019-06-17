/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility/KxFileSystem/KxFileItem.h"

namespace KxVFS
{
	class KxVFS_API FileItemWith_WIN32_FIND_DATA: public KxFileItemBase
	{
		protected:
			WIN32_FIND_DATAW m_FindData = {};

		protected:
			void OnChange() override
			{
				ToWIN32_FIND_DATA(m_FindData);
			}

		public:
			FileItemWith_WIN32_FIND_DATA() = default;
			FileItemWith_WIN32_FIND_DATA(KxDynamicStringRefW fileName)
				:KxFileItemBase(fileName)
			{
				ToWIN32_FIND_DATA(m_FindData);
			}
			FileItemWith_WIN32_FIND_DATA(const KxFileItemBase& item)
				:KxFileItemBase(item)
			{
				ToWIN32_FIND_DATA(m_FindData);
			}
			FileItemWith_WIN32_FIND_DATA(KxFileItemBase&& item)
				:KxFileItemBase(std::move(item))
			{
				ToWIN32_FIND_DATA(m_FindData);
			}

		public:
			FileItemWith_WIN32_FIND_DATA& SetFile()
			{
				KxFileItemBase::SetFile();
				return *this;
			}
			FileItemWith_WIN32_FIND_DATA& SetDirectory()
			{
				KxFileItemBase::SetDirectory();
				return *this;
			}
			FileItemWith_WIN32_FIND_DATA& SetReadOnly(bool value = true)
			{
				KxFileItemBase::SetReadOnly(value);
				return *this;
			}
	
			const WIN32_FIND_DATAW& AsWIN32_FIND_DATA() const
			{
				return m_FindData;
			}
			WIN32_FIND_DATAW& AsWIN32_FIND_DATA()
			{
				return m_FindData;
			}
	};
}
