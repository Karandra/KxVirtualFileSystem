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

	HANDLE CallFindFirstFile(KxDynamicStringRefW query, WIN32_FIND_DATAW& fileInfo, bool isCaseSensitive, bool queryShortNames)
	{
		const DWORD searchFlags = FIND_FIRST_EX_LARGE_FETCH|(isCaseSensitive ? FIND_FIRST_EX_CASE_SENSITIVE : 0);
		const FINDEX_INFO_LEVELS infoLevel = queryShortNames ? FindExInfoStandard : FindExInfoBasic;
		return ::FindFirstFileExW(query.data(), infoLevel, &fileInfo, FindExSearchNameMatch, nullptr, searchFlags);
	}
	bool CallFindNextFile(HANDLE handle, WIN32_FIND_DATAW& fileInfo)
	{
		return ::FindNextFileW(handle, &fileInfo);
	}
	bool CallFindClose(HANDLE handle)
	{
		return ::FindClose(handle);
	}
}

namespace KxVFS::Utility
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
	KxFileFinder::~KxFileFinder()
	{
		// If sequential search is not completed, close the handle here
		if (m_Handle != INVALID_HANDLE_VALUE)
		{
			CallFindClose(m_Handle);
		}
	}

	bool KxFileFinder::IsOK() const
	{
		return m_Handle != INVALID_HANDLE_VALUE && m_Handle != nullptr;
	}
	bool KxFileFinder::Run()
	{
		WIN32_FIND_DATAW fileInfo = {0};
		HANDLE searchHandle = CallFindFirstFile(m_SearchQuery, fileInfo, m_CaseSensitive, m_QueryShortNames);
		if (searchHandle != INVALID_HANDLE_VALUE)
		{
			if (OnFound(fileInfo))
			{
				while (CallFindNextFile(searchHandle, fileInfo) && OnFound(fileInfo))
				{
				}
			}
			CallFindClose(searchHandle);
			return true;
		}
		return false;
	}
	KxFileItem KxFileFinder::FindNext()
	{
		if (m_Handle == INVALID_HANDLE_VALUE)
		{
			// No search handle available, begin operation.
			m_Handle = CallFindFirstFile(m_SearchQuery, m_FindData, m_CaseSensitive, m_QueryShortNames);
			if (m_Handle != INVALID_HANDLE_VALUE)
			{
				m_IsCanceled = false;
				return KxFileItem(*this, m_FindData);
			}
		}
		else
		{
			// We have handle, find next file.
			if (CallFindNextFile(m_Handle, m_FindData))
			{
				return KxFileItem(*this, m_FindData);
			}

			// No files left, close search handle
			CallFindClose(m_Handle);
			m_Handle = INVALID_HANDLE_VALUE;
			m_IsCanceled = false;
		}
		return KxFileItem();
	}
}
