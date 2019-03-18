/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxVirtualFileSystem/Utility/SearchHandle.h"
#include "KxIFileFinder.h"
#include "KxFileItem.h"

namespace KxVFS
{
	class KxVFS_API KxFileFinder: public KxIFileFinder
	{
		public:
			static bool IsDirectoryEmpty(KxDynamicStringRefW directoryPath);

		private:
			KxDynamicStringW m_SearchQuery;
			bool m_IsCanceled = false;
			bool m_CaseSensitive = false;
			bool m_QueryShortNames = false;

			SearchHandle m_Handle;
			WIN32_FIND_DATAW m_FindData = {0};

		protected:
			bool OnFound(const KxFileItem& foundItem) override;
			bool OnFound(const WIN32_FIND_DATAW& fileInfo);

		public:
			KxFileFinder(KxDynamicStringRefW searchQuery);
			KxFileFinder(KxDynamicStringRefW source, KxDynamicStringRefW filter);
			virtual ~KxFileFinder() = default;

		public:
			bool IsOK() const override;
			bool IsCanceled() const override
			{
				return m_IsCanceled;
			}
		
			bool IsCaseSensitive() const
			{
				return m_CaseSensitive;
			}
			void SetCaseSensitive(bool value = true)
			{
				m_CaseSensitive = value;
			}

			bool ShouldQueryShortNames() const
			{
				return m_QueryShortNames;
			}
			void QueryShortNames(bool value = true)
			{
				m_QueryShortNames = value;
			}

			bool Run() override;
			KxFileItem FindNext() override;
			void NotifyFound(const KxFileItem& foundItem)
			{
				m_IsCanceled = !OnFound(foundItem);
			}

			KxDynamicStringW GetSource() const
			{
				return ExtrctSourceFromSearchQuery(m_SearchQuery);
			}
	};
}
