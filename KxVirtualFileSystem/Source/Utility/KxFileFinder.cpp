#include "KxVirtualFileSystem.h"
#include "KxFileFinder.h"

bool KxFileFinder::IsDirectoryEmpty(const KxDynamicStringRef& directoryPath)
{
	KxFileFinder finder(directoryPath, TEXT("*"));

	KxFileFinderItem item = finder.FindNext();
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
	KxFileFinderItem foundItem(this, fileInfo);
	if (!foundItem.IsCurrentOrParent())
	{
		return OnFound(foundItem);
	}
	return true;
}
bool KxFileFinder::OnFound(const KxFileFinderItem& foundItem)
{
	return true;
}
KxDynamicString KxFileFinder::ConstructSearchQuery() const
{
	if (!m_Filter.empty())
	{
		KxDynamicString query = m_Source;
		query += TEXT('\\');
		query += m_Filter;
		return query;
	}
	return m_Source;
}

KxFileFinder::KxFileFinder(const KxDynamicString& source, const KxDynamicStringRef& filter)
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
	return m_Handle != INVALID_HANDLE_VALUE && m_Handle != NULL;
}
bool KxFileFinder::Run()
{
	WIN32_FIND_DATAW fileInfo = {0};
	KxDynamicString query = ConstructSearchQuery();
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
KxFileFinderItem KxFileFinder::FindNext()
{
	if (!IsOK())
	{
		// No search handle available, begin operation.
		KxDynamicString query = ConstructSearchQuery();
		m_Handle = ::FindFirstFileW(query, &m_FindData);
		if (IsOK())
		{
			m_Canceled = false;
			return KxFileFinderItem(this, m_FindData);
		}
	}
	else
	{
		// We have handle, find next file.
		if (::FindNextFileW(m_Handle, &m_FindData))
		{
			return KxFileFinderItem(this, m_FindData);
		}
		
		::FindClose(m_Handle);
		m_Canceled = false;
		m_Handle = INVALID_HANDLE_VALUE;
	}
	return KxFileFinderItem();
}

//////////////////////////////////////////////////////////////////////////
void KxFileFinderItem::MakeNull(bool attribuesOnly)
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
		*this = KxFileFinderItem();
	}
}
void KxFileFinderItem::Set(const WIN32_FIND_DATAW& fileInfo)
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

KxFileFinderItem::KxFileFinderItem(const KxDynamicStringRef& fullPath)
	:m_Source(fullPath)
{
	m_Source = m_Source.before_last('\\', &m_Name);

	UpdateInfo();
}
KxFileFinderItem::KxFileFinderItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo)
	:m_Source(finder->GetSource())
{
	Set(fileInfo);
}
KxFileFinderItem::~KxFileFinderItem()
{
}

bool KxFileFinderItem::IsCurrentOrParent() const
{
	const KxDynamicString::CharT c1 = m_Name[0];
	const KxDynamicString::CharT c2 = m_Name[1];
	const KxDynamicString::CharT c3 = m_Name[2];

	return (c1 == TEXT('.') && c2 == TEXT('.') && c3 == TEXT('\000')) || (c1 == TEXT('.') && c2 == TEXT('\000'));
}
bool KxFileFinderItem::UpdateInfo()
{
	KxDynamicString query = m_Source;
	query += TEXT('\\');
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
WIN32_FIND_DATAW KxFileFinderItem::GetAsWIN32_FIND_DATA() const
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
