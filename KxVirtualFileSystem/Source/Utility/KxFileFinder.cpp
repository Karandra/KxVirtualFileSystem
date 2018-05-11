#include "KxVirtualFileSystem.h"
#include "KxFileFinder.h"

bool KxFileFinder::IsDirectoryEmpty(const KxDynamicStringRef& sDirectoryPath)
{
	KxFileFinder tFinder(sDirectoryPath, TEXT("*"));

	KxFileFinderItem tItem = tFinder.FindNext();
	if (!tItem.IsOK())
	{
		// No files at all, folder is empty.
		return true;
	}
	else if (tItem.IsCurrentOrParent())
	{
		// If first and second item are references to current and parent folders
		// and no more files found, then this particular folder is empty.
		tItem = tFinder.FindNext();
		if (tItem.IsCurrentOrParent())
		{
			return !tFinder.FindNext().IsOK();
		}
	}
	return false;
}

bool KxFileFinder::OnFound(const WIN32_FIND_DATAW& tFileInfo)
{
	KxFileFinderItem tFoundItem(this, tFileInfo);
	if (!tFoundItem.IsCurrentOrParent())
	{
		return OnFound(tFoundItem);
	}
	return true;
}
bool KxFileFinder::OnFound(const KxFileFinderItem& tFoundItem)
{
	return true;
}
KxDynamicString KxFileFinder::ConstructSearchQuery() const
{
	if (!m_Filter.empty())
	{
		KxDynamicString sQuery = m_Source;
		sQuery += TEXT('\\');
		sQuery += m_Filter;
		return sQuery;
	}
	return m_Source;
}

KxFileFinder::KxFileFinder(const KxDynamicString& sSource, const KxDynamicStringRef& sFilter)
	:m_Source(sSource), m_Filter(sFilter)
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
	WIN32_FIND_DATAW tFileInfo = {0};
	KxDynamicString sQuery = ConstructSearchQuery();
	HANDLE hSearchHandle = ::FindFirstFileW(sQuery, &tFileInfo);
	if (hSearchHandle != INVALID_HANDLE_VALUE)
	{
		if (OnFound(tFileInfo))
		{
			while (::FindNextFileW(hSearchHandle, &tFileInfo) && OnFound(tFileInfo))
			{
			}
		}
		::FindClose(hSearchHandle);
		return true;
	}
	return false;
}
KxFileFinderItem KxFileFinder::FindNext()
{
	if (!IsOK())
	{
		// No search handle available, begin operation.
		KxDynamicString sQuery = ConstructSearchQuery();
		m_Handle = ::FindFirstFileW(sQuery, &m_FindData);
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
void KxFileFinderItem::MakeNull(bool bAttribuesOnly)
{
	if (bAttribuesOnly)
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
void KxFileFinderItem::Set(const WIN32_FIND_DATAW& tFileInfo)
{
	m_Attributes = tFileInfo.dwFileAttributes;
	if (IsReparsePoint())
	{
		m_ReparsePointAttributes = tFileInfo.dwReserved0;
	}

	if (IsFile())
	{
		ULARGE_INTEGER tSize = {0};
		tSize.HighPart = tFileInfo.nFileSizeHigh;
		tSize.LowPart = tFileInfo.nFileSizeLow;
		m_FileSize = (int64_t)tSize.QuadPart;
	}
	else
	{
		m_FileSize = -1;
	}

	m_Name = tFileInfo.cFileName;

	m_CreationTime = tFileInfo.ftCreationTime;
	m_LastAccessTime = tFileInfo.ftLastAccessTime;
	m_ModificationTime = tFileInfo.ftLastWriteTime;
}

KxFileFinderItem::KxFileFinderItem(const KxDynamicStringRef& sFullPath)
	:m_Source(sFullPath)
{
	m_Source = m_Source.before_last('\\', &m_Name);

	UpdateInfo();
}
KxFileFinderItem::KxFileFinderItem(KxFileFinder* pFinder, const WIN32_FIND_DATAW& tFileInfo)
	:m_Source(pFinder->GetSource())
{
	Set(tFileInfo);
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
	KxDynamicString sQuery = m_Source;
	sQuery += TEXT('\\');
	sQuery += m_Name;

	WIN32_FIND_DATAW tInfo = {0};
	HANDLE hSearchHandle = ::FindFirstFileW(sQuery, &tInfo);
	if (hSearchHandle != INVALID_HANDLE_VALUE)
	{
		Set(tInfo);
		::FindClose(hSearchHandle);
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
	WIN32_FIND_DATAW tFindData = {};

	tFindData.dwFileAttributes = m_Attributes;
	tFindData.ftCreationTime = m_CreationTime;
	tFindData.ftLastWriteTime = m_ModificationTime;
	tFindData.ftLastAccessTime = m_LastAccessTime;

	if (IsReparsePoint())
	{
		tFindData.dwReserved0 = m_ReparsePointAttributes;
	}
	
	ULARGE_INTEGER tSize = {0};
	tSize.QuadPart = m_FileSize;

	tFindData.nFileSizeHigh = tSize.HighPart;
	tFindData.nFileSizeLow = tSize.LowPart;

	wcsncpy_s(tFindData.cFileName, m_Name, m_Name.length());

	return tFindData;
}
