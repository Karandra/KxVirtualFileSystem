/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxIFileFinder.h"
#include "KxFileFinder.h"
#include "KxFileItem.h"

namespace
{
	using namespace KxVFS;

	SearchHandle BeginSearch(KxDynamicStringRefW query, WIN32_FIND_DATAW& fileInfo, bool isCaseSensitive, bool queryShortNames)
	{
		const DWORD searchFlags = FIND_FIRST_EX_LARGE_FETCH|(isCaseSensitive ? FIND_FIRST_EX_CASE_SENSITIVE : 0);
		const FINDEX_INFO_LEVELS infoLevel = queryShortNames ? FindExInfoStandard : FindExInfoBasic;
		return ::FindFirstFileExW(query.data(), infoLevel, &fileInfo, FindExSearchNameMatch, nullptr, searchFlags);
	}
	bool ContinueSearch(SearchHandle& handle, WIN32_FIND_DATAW& fileInfo)
	{
		return ::FindNextFileW(handle, &fileInfo);
	}
}

namespace KxVFS
{
	bool KxFileFinder::IsDirectoryEmpty(KxDynamicStringRefW directoryPath)
	{
		KxFileFinder finder(directoryPath, L"*");

		KxFileItem item = finder.FindNext();
		if (!item.IsOK())
		{
			// No files at all, folder is empty.
			return true;
		}
		else if (item.IsCurrentOrParent())
		{
			// If first and second item are references to current and parent folders
			// and no more files found, then this particular folder is empty.
			item = finder.FindNext();
			if (item.IsCurrentOrParent())
			{
				return !finder.FindNext().IsOK();
			}
		}
		return false;
	}

	bool KxFileFinder::OnFound(const WIN32_FIND_DATAW& fileInfo)
	{
		KxFileItem foundItem(*this, fileInfo);
		if (!foundItem.IsCurrentOrParent())
		{
			return OnFound(foundItem);
		}
		return true;
	}
	bool KxFileFinder::OnFound(const KxFileItem& foundItem)
	{
		return true;
	}

	KxFileFinder::KxFileFinder(KxDynamicStringRefW searchQuery)
		:m_SearchQuery(Normalize(searchQuery, true, true))
	{
	}
	KxFileFinder::KxFileFinder(KxDynamicStringRefW source, KxDynamicStringRefW filter)
		:m_SearchQuery(Normalize(ConstructSearchQuery(source, filter), true, true))
	{
	}

	bool KxFileFinder::IsOK() const
	{
		return m_Handle.IsValid() && !m_Handle.IsNull();
	}
	bool KxFileFinder::Run()
	{
		WIN32_FIND_DATAW fileInfo = {0};
		SearchHandle searchHandle = BeginSearch(m_SearchQuery, fileInfo, m_CaseSensitive, m_QueryShortNames);
		if (searchHandle)
		{
			if (OnFound(fileInfo))
			{
				while (ContinueSearch(searchHandle, fileInfo) && OnFound(fileInfo))
				{
				}
			}
			return true;
		}
		return false;
	}
	KxFileItem KxFileFinder::FindNext()
	{
		if (!m_Handle.IsValid())
		{
			// No search handle available, begin operation.
			m_Handle = BeginSearch(m_SearchQuery, m_FindData, m_CaseSensitive, m_QueryShortNames);
			if (m_Handle)
			{
				m_IsCanceled = false;
				return KxFileItem(*this, m_FindData);
			}
		}
		else
		{
			// We have handle, find next file.
			if (ContinueSearch(m_Handle, m_FindData))
			{
				return KxFileItem(*this, m_FindData);
			}

			// No files left, close search handle
			m_Handle.Close();
			m_IsCanceled = false;
		}
		return KxFileItem();
	}
}
