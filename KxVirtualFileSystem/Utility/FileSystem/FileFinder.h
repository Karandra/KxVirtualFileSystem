#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility.h"
#include "KxVirtualFileSystem/Utility/SearchHandle.h"
#include "IFileFinder.h"
#include "FileItem.h"

namespace KxVFS
{
	class KxVFS_API FileFinder: public IFileFinder
	{
		public:
			static bool IsDirectoryEmpty(DynamicStringRefW directoryPath);

		private:
			DynamicStringW m_SearchQuery;
			bool m_IsCanceled = false;
			bool m_CaseSensitive = false;
			bool m_QueryShortNames = false;

			SearchHandle m_Handle;
			WIN32_FIND_DATAW m_FindData = {0};

		protected:
			bool OnFound(const FileItem& foundItem) override;
			bool OnFound(const WIN32_FIND_DATAW& fileInfo);

		public:
			FileFinder(DynamicStringRefW searchQuery);
			FileFinder(DynamicStringRefW source, DynamicStringRefW filter);
			virtual ~FileFinder() = default;

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
			FileItem FindNext() override;
			void NotifyFound(const FileItem& foundItem)
			{
				m_IsCanceled = !OnFound(foundItem);
			}

			DynamicStringW GetSource() const
			{
				return ExtrctSourceFromSearchQuery(m_SearchQuery);
			}
	};
}
