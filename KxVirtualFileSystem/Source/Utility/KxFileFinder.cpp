/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxFileFinder.h"

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
		KxFileItem foundItem(this, fileInfo);
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
	KxDynamicStringW KxFileFinder::ConstructSearchQuery() const
	{
		if (!m_Filter.empty())
		{
			KxDynamicStringW query = m_Source;
			query += TEXT('\\');
			query += m_Filter;
			return query;
		}
		return m_Source;
	}

	KxFileFinder::KxFileFinder(KxDynamicStringRefW source, KxDynamicStringRefW filter)
		:m_Source(source), m_Filter(filter)
	{
	}
	KxFileFinder::~KxFileFinder()
	{
		// If sequential search is not completed, close the handle here
		if (IsOK())
		{
			::FindClose(m_Handle);
		}
	}

	bool KxFileFinder::IsOK() const
	{
		return m_Handle != INVALID_HANDLE_VALUE && m_Handle != nullptr;
	}
	bool KxFileFinder::Run()
	{
		WIN32_FIND_DATAW fileInfo = {0};
		KxDynamicStringW query = ConstructSearchQuery();
		HANDLE searchHandle = ::FindFirstFileW(query, &fileInfo);
		if (searchHandle != INVALID_HANDLE_VALUE)
		{
			if (OnFound(fileInfo))
			{
				while (::FindNextFileW(searchHandle, &fileInfo) && OnFound(fileInfo))
				{
				}
			}
			::FindClose(searchHandle);
			return true;
		}
		return false;
	}
	KxFileItem KxFileFinder::FindNext()
	{
		if (!IsOK())
		{
			// No search handle available, begin operation.
			KxDynamicStringW query = ConstructSearchQuery();
			m_Handle = ::FindFirstFileW(query, &m_FindData);
			if (IsOK())
			{
				m_IsCanceled = false;
				return KxFileItem(this, m_FindData);
			}
		}
		else
		{
			// We have handle, find next file.
			if (::FindNextFileW(m_Handle, &m_FindData))
			{
				return KxFileItem(this, m_FindData);
			}

			::FindClose(m_Handle);
			m_IsCanceled = false;
			m_Handle = INVALID_HANDLE_VALUE;
		}
		return KxFileItem();
	}
}
	
namespace KxVFS
{
	void KxFileItem::MakeNull(bool attribuesOnly)
	{
		if (attribuesOnly)
		{
			m_Attributes = INVALID_FILE_ATTRIBUTES;
			m_ReparsePointAttributes = 0;
			m_CreationTime = {};
			m_LastAccessTime = {};
			m_ModificationTime = {};
			m_FileSize = -1;
		}
		else
		{
			*this = KxFileItem();
		}
	}
	void KxFileItem::Set(const WIN32_FIND_DATAW& fileInfo)
	{
		m_Attributes = fileInfo.dwFileAttributes;
		if (IsReparsePoint())
		{
			m_ReparsePointAttributes = fileInfo.dwReserved0;
		}

		if (IsFile())
		{
			ULARGE_INTEGER size = {0};
			size.HighPart = fileInfo.nFileSizeHigh;
			size.LowPart = fileInfo.nFileSizeLow;
			m_FileSize = (int64_t)size.QuadPart;
		}
		else
		{
			m_FileSize = -1;
		}

		m_Name = fileInfo.cFileName;

		m_CreationTime = fileInfo.ftCreationTime;
		m_LastAccessTime = fileInfo.ftLastAccessTime;
		m_ModificationTime = fileInfo.ftLastWriteTime;
	}

	KxFileItem::KxFileItem(KxDynamicStringRefW fullPath)
		:m_Source(fullPath)
	{
		m_Source = m_Source.before_last(L'\\', &m_Name);

		UpdateInfo();
	}
	KxFileItem::KxFileItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo)
		:m_Source(finder->GetSource())
	{
		Set(fileInfo);
	}

	bool KxFileItem::IsCurrentOrParent() const
	{
		const wchar_t c1 = m_Name[0];
		const wchar_t c2 = m_Name[1];
		const wchar_t c3 = m_Name[2];

		return (c1 == L'.' && c2 == L'.' && c3 == L'\000') || (c1 == L'.' && c2 == L'\000');
	}
	bool KxFileItem::UpdateInfo()
	{
		KxDynamicStringW query = m_Source;
		query += L'\\';
		query += m_Name;

		WIN32_FIND_DATAW info = {0};
		HANDLE searchHandle = ::FindFirstFileW(query, &info);
		if (searchHandle != INVALID_HANDLE_VALUE)
		{
			Set(info);
			::FindClose(searchHandle);
			return true;
		}
		else
		{
			MakeNull(true);
			return false;
		}
	}
	
	WIN32_FIND_DATAW KxFileItem::GetAsWIN32_FIND_DATA() const
	{
		WIN32_FIND_DATAW findData = {};

		findData.dwFileAttributes = m_Attributes;
		findData.ftCreationTime = m_CreationTime;
		findData.ftLastWriteTime = m_ModificationTime;
		findData.ftLastAccessTime = m_LastAccessTime;

		if (IsReparsePoint())
		{
			findData.dwReserved0 = m_ReparsePointAttributes;
		}

		ULARGE_INTEGER size = {0};
		size.QuadPart = m_FileSize;

		findData.nFileSizeHigh = size.HighPart;
		findData.nFileSizeLow = size.LowPart;

		wcsncpy_s(findData.cFileName, m_Name, m_Name.length());

		return findData;
	}
}
