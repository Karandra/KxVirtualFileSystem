/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxFileItem.h"
#include "KxFileFinder.h"

namespace KxVFS::Utility
{
	void KxFileItem::MakeNull(bool attribuesOnly)
	{
		if (attribuesOnly)
		{
			m_Attributes = INVALID_FILE_ATTRIBUTES;
			m_ReparsePointAttributes = 0;
			m_CreationTime = {0};
			m_LastAccessTime = {0};
			m_ModificationTime = {0};
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

		m_FileSize = -1;
		if (IsFile())
		{
			Utility::HighLowToInt64(m_FileSize, fileInfo.nFileSizeHigh, fileInfo.nFileSizeLow);
		}

		m_Name = fileInfo.cFileName;
		m_CreationTime = fileInfo.ftCreationTime;
		m_LastAccessTime = fileInfo.ftLastAccessTime;
		m_ModificationTime = fileInfo.ftLastWriteTime;
	}
	bool KxFileItem::DoUpdateInfo(KxDynamicStringRefW fullPath)
	{
		WIN32_FIND_DATAW info = {0};
		HANDLE searchHandle = ::FindFirstFileExW(fullPath.data(), FindExInfoBasic, &info, FindExSearchNameMatch, nullptr, 0);
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

	KxFileItem::KxFileItem(KxDynamicStringRefW fullPath)
	{
		SetFullPath(fullPath);
		DoUpdateInfo(fullPath);
	}
	KxFileItem::KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName)
		:m_Source(source), m_Name(fileName)
	{
		UpdateInfo();
	}
	KxFileItem::KxFileItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo)
		: m_Source(finder->GetSource())
	{
		Set(fileInfo);
	}

	bool KxFileItem::IsCurrentOrParent() const
	{
		return m_Name == L".." || m_Name == L".";
	}
	bool KxFileItem::IsDirectoryEmpty() const
	{
		return IsDirectory() && KxFileFinder::IsDirectoryEmpty(m_Source);
	}

	KxDynamicStringW KxFileItem::GetFileExtension() const
	{
		if (IsFile())
		{
			const size_t pos = m_Name.rfind(L'.');
			if (pos != KxDynamicStringW::npos)
			{
				return m_Name.substr(pos + 1);
			}
		}
		return KxDynamicStringW();
	}
	void KxFileItem::SetFileExtension(KxDynamicStringRefW ext)
	{
		if (IsFile())
		{
			const size_t pos = m_Name.rfind(L'.');
			if (pos != KxDynamicStringRefW::npos)
			{
				// KxDynamicString don't have 'replace' function
				m_Name.resize(std::max(pos + ext.length() + 1, m_Name.length()));
				for (size_t i = 0; i < ext.length(); i++)
				{
					m_Name[pos + 1 + i] = ext[i];
				}
			}
			else
			{
				m_Name += L'.';
				m_Name += ext;
			}
		}
	}

	WIN32_FIND_DATAW KxFileItem::ToWIN32_FIND_DATA() const
	{
		WIN32_FIND_DATAW findData = {0};

		// File name
		wcsncpy_s(findData.cFileName, m_Name.data(), m_Name.size());

		// Attributes
		findData.dwFileAttributes = m_Attributes;
		if (IsReparsePoint())
		{
			findData.dwReserved0 = m_ReparsePointAttributes;
		}

		// Time
		findData.ftCreationTime = m_CreationTime;
		findData.ftLastAccessTime = m_LastAccessTime;
		findData.ftLastWriteTime = m_ModificationTime;

		// File size
		Utility::Int64ToHighLow(m_FileSize, findData.nFileSizeHigh, findData.nFileSizeLow);
		
		return findData;
	}
}
