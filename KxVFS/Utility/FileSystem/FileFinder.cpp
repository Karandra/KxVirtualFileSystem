#include "stdafx.h"
#include "KxVFS/Utility.h"
#include "IFileFinder.h"
#include "FileFinder.h"
#include "FileItem.h"

namespace
{
	using namespace KxVFS;

	SearchHandle BeginSearch(DynamicStringRefW query, WIN32_FIND_DATAW& fileInfo, bool isCaseSensitive, bool queryShortNames) noexcept
	{
		const DWORD searchFlags = FIND_FIRST_EX_LARGE_FETCH|(isCaseSensitive ? FIND_FIRST_EX_CASE_SENSITIVE : 0);
		const FINDEX_INFO_LEVELS infoLevel = queryShortNames ? FindExInfoStandard : FindExInfoBasic;
		return ::FindFirstFileExW(query.data(), infoLevel, &fileInfo, FindExSearchNameMatch, nullptr, searchFlags);
	}
	bool ContinueSearch(SearchHandle& handle, WIN32_FIND_DATAW& fileInfo) noexcept
	{
		return ::FindNextFileW(handle, &fileInfo);
	}
}

namespace KxVFS
{
	bool FileFinder::IsDirectoryEmpty(DynamicStringRefW directoryPath)
	{
		FileFinder finder(directoryPath, L"*");

		FileItem item = finder.FindNext();
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

	bool FileFinder::OnFound(const WIN32_FIND_DATAW& fileInfo)
	{
		FileItem foundItem(*this, fileInfo);
		if (!foundItem.IsCurrentOrParent())
		{
			return OnFound(foundItem);
		}
		return true;
	}
	bool FileFinder::OnFound(const FileItem& foundItem)
	{
		return true;
	}

	FileFinder::FileFinder(DynamicStringRefW searchQuery)
		:m_SearchQuery(Normalize(searchQuery, true, true))
	{
	}
	FileFinder::FileFinder(DynamicStringRefW source, DynamicStringRefW filter)
		:m_SearchQuery(Normalize(ConstructSearchQuery(source, filter), true, true))
	{
	}

	bool FileFinder::IsOK() const
	{
		return !m_Handle.IsNull() && !m_Handle.IsNull();
	}
	bool FileFinder::Run()
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
	FileItem FileFinder::FindNext()
	{
		if (!m_Handle)
		{
			// No search handle available, begin operation.
			m_Handle = BeginSearch(m_SearchQuery, m_FindData, m_CaseSensitive, m_QueryShortNames);
			if (m_Handle)
			{
				m_IsCanceled = false;
				return FileItem(*this, m_FindData);
			}
		}
		else
		{
			// We have handle, find next file.
			if (ContinueSearch(m_Handle, m_FindData))
			{
				return FileItem(*this, m_FindData);
			}

			// No files left, close search handle
			m_Handle.Close();
			m_IsCanceled = false;
		}
		return {};
	}
}
