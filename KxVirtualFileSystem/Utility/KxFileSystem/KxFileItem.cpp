/*
Copyright � 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxFileItem.h"
#include "KxFileFinder.h"

namespace KxVFS
{
	void KxFileItem::MakeNull(bool attribuesOnly)
	{
		if (attribuesOnly)
		{
			m_Attributes = FileAttributes::Invalid;
			m_ReparsePointTags = ReparsePointTags::None;
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
	bool KxFileItem::DoUpdateInfo(KxDynamicStringRefW fullPath, bool queryShortName)
	{
		KxFileFinder finder(fullPath);
		finder.QueryShortNames(queryShortName);

		KxFileItem item = finder.FindNext();
		if (item.IsOK())
		{
			*this = std::move(item);
			return true;
		}
		else
		{
			MakeNull(true);
			return false;
		}
	}
	KxDynamicStringRefW KxFileItem::TrimNamespace(KxDynamicStringRefW path) const
	{
		const size_t prefixLength = Utility::GetLongPathPrefix().size();
		if (path.substr(0, prefixLength) == Utility::GetLongPathPrefix())
		{
			path.remove_prefix(prefixLength);
		}
		return path;
	}

	KxFileItem::KxFileItem(KxDynamicStringRefW fullPath)
	{
		SetFullPath(fullPath);
		DoUpdateInfo(fullPath);
	}
	KxFileItem::KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName)
		:m_Source(TrimNamespace(source)), m_Name(fileName)
	{
		UpdateInfo();
	}
	KxFileItem::KxFileItem(const KxFileFinder& finder, const WIN32_FIND_DATAW& fileInfo)
		:m_Source(TrimNamespace(finder.GetSource()))
	{
		FromWIN32_FIND_DATA(fileInfo);
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

	void KxFileItem::FromWIN32_FIND_DATA(const WIN32_FIND_DATAW& findInfo)
	{
		m_Attributes = FromInt<FileAttributes>(findInfo.dwFileAttributes);
		if (IsReparsePoint())
		{
			m_ReparsePointTags = FromInt<ReparsePointTags>(findInfo.dwReserved0);
		}

		m_FileSize = -1;
		if (IsFile())
		{
			Utility::HighLowToInt64(m_FileSize, findInfo.nFileSizeHigh, findInfo.nFileSizeLow);
		}

		m_Name = findInfo.cFileName;
		m_ShortName = findInfo.cAlternateFileName;
		m_CreationTime = findInfo.ftCreationTime;
		m_LastAccessTime = findInfo.ftLastAccessTime;
		m_ModificationTime = findInfo.ftLastWriteTime;
	}
	void KxFileItem::ToWIN32_FIND_DATA(WIN32_FIND_DATAW& findData) const
	{
		// File name
		wcsncpy_s(findData.cFileName, m_Name, m_Name.size());
		wcsncpy_s(findData.cAlternateFileName, m_ShortName, m_ShortName.size());

		// Attributes
		findData.dwFileAttributes = ToInt(m_Attributes);
		findData.dwReserved0 = IsReparsePoint() ? ToInt(m_ReparsePointTags) : 0;

		// Time
		findData.ftCreationTime = m_CreationTime;
		findData.ftLastAccessTime = m_LastAccessTime;
		findData.ftLastWriteTime = m_ModificationTime;

		// File size
		Utility::Int64ToHighLow(IsFile() ? m_FileSize : 0, findData.nFileSizeHigh, findData.nFileSizeLow);
	}

	void KxFileItem::ToBY_HANDLE_FILE_INFORMATION(BY_HANDLE_FILE_INFORMATION& fileInfo) const
	{
		fileInfo.dwFileAttributes = ToInt(m_Attributes);
		fileInfo.ftCreationTime = m_CreationTime;
		fileInfo.ftLastAccessTime = m_LastAccessTime;
		fileInfo.ftLastWriteTime = m_ModificationTime;

		if (IsFile())
		{
			Utility::Int64ToHighLow(m_FileSize, fileInfo.nFileSizeHigh, fileInfo.nFileSizeLow);
		}
		else
		{
			fileInfo.nFileIndexLow = 0;
			fileInfo.nFileSizeHigh = 0;
		}
	}
	void KxFileItem::FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& fileInfo)
	{
		m_Attributes = FromInt<FileAttributes>(fileInfo.dwFileAttributes);
		m_ReparsePointTags = ReparsePointTags::None;
		m_CreationTime = fileInfo.ftCreationTime;
		m_LastAccessTime = fileInfo.ftLastAccessTime;
		m_ModificationTime = fileInfo.ftLastWriteTime;

		if (IsFile())
		{
			Utility::HighLowToInt64(m_FileSize, fileInfo.nFileSizeHigh, fileInfo.nFileSizeLow);
		}
		else
		{
			m_FileSize = 0;
		}
	}
	void KxFileItem::FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& fileInfo)
	{
		m_Attributes = FromInt<FileAttributes>(fileInfo.FileAttributes);
		m_ReparsePointTags = ReparsePointTags::None;
		m_CreationTime = FileTimeFromLARGE_INTEGER(fileInfo.CreationTime);
		m_LastAccessTime = FileTimeFromLARGE_INTEGER(fileInfo.LastAccessTime);
		m_ModificationTime = FileTimeFromLARGE_INTEGER(fileInfo.LastWriteTime);
	}
}
