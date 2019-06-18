/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxFileItem.h"
#include "KxFileFinder.h"

namespace KxVFS
{
	void KxFileItem::ExtractSourceAndName(KxDynamicStringRefW fullPath, KxDynamicStringW& source, KxDynamicStringW& name)
	{
		source = KxDynamicStringW(fullPath).before_last(L'\\', &name);
		source = TrimNamespace(source);
	}
	KxDynamicStringRefW KxFileItem::TrimNamespace(KxDynamicStringRefW path) noexcept
	{
		const size_t prefixLength = Utility::GetLongPathPrefix().size();
		if (path.substr(0, prefixLength) == Utility::GetLongPathPrefix())
		{
			path.remove_prefix(prefixLength);
		}
		return path;
	}

	KxFileItem::KxFileItem(const KxFileFinder& finder, const Win32FindData& findData)
		:m_Data(findData), m_Source(TrimNamespace(finder.GetSource()))
	{
	}
	KxFileItem::KxFileItem(const KxFileFinder& finder, const WIN32_FIND_DATAW& findData)
		:m_NativeData(findData), m_Source(TrimNamespace(finder.GetSource()))
	{
	}

	void KxFileItem::MakeNull(bool attribuesOnly) noexcept
	{
		if (attribuesOnly)
		{
			m_Data.m_Attributes = FileAttributes::Invalid;
			m_Data.m_ReparsePointTags = ReparsePointTags::None;
			m_Data.m_CreationTime = {0};
			m_Data.m_LastAccessTime = {0};
			m_Data.m_ModificationTime = {0};
			m_Data.SetFileSize(0);
		}
		else
		{
			m_Data = {};
			m_Source.clear();
		}
		OnChange();
	}
	bool KxFileItem::UpdateInfo(KxDynamicStringRefW fullPath, bool queryShortName)
	{
		KxCallAtScopeExit atExit([this]()
		{
			OnChange();
		});
		
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
	bool KxFileItem::IsDirectoryEmpty() const
	{
		return IsDirectory() && KxFileFinder::IsDirectoryEmpty(m_Source);
	}

	KxDynamicStringW KxFileItem::GetFileExtension() const noexcept
	{
		if (IsFile())
		{
			const KxDynamicStringRefW name = GetName();
			const size_t pos = name.rfind(L'.');
			if (pos != KxDynamicStringW::npos)
			{
				return name.substr(pos + 1);
			}
		}
		return {};
	}
	void KxFileItem::SetFileExtension(KxDynamicStringRefW ext)
	{
		if (IsFile())
		{
			KxDynamicStringW name = GetName();
			const size_t pos = name.rfind(L'.');
			if (pos != KxDynamicStringRefW::npos)
			{
				// KxDynamicString don't have 'replace' function
				name.resize(std::max(pos + ext.length() + 1, name.length()));
				for (size_t i = 0; i < ext.length(); i++)
				{
					name[pos + 1 + i] = ext[i];
				}
			}
			else
			{
				name += L'.';
				name += ext;
			}

			SetName(name);
			OnChange();
		}
	}

	void KxFileItem::ToBY_HANDLE_FILE_INFORMATION(BY_HANDLE_FILE_INFORMATION& fileInfo) const noexcept
	{
		fileInfo.dwFileAttributes = ToInt(m_Data.m_Attributes);
		fileInfo.ftCreationTime = m_Data.m_CreationTime;
		fileInfo.ftLastAccessTime = m_Data.m_LastAccessTime;
		fileInfo.ftLastWriteTime = m_Data.m_ModificationTime;

		if (IsFile())
		{
			fileInfo.nFileSizeLow = m_NativeData.nFileSizeLow;
			fileInfo.nFileSizeHigh = m_NativeData.nFileSizeHigh;
		}
		else
		{
			fileInfo.nFileSizeLow = 0;
			fileInfo.nFileSizeHigh = 0;
		}
	}
	void KxFileItem::FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& fileInfo) noexcept
	{
		m_Data.m_Attributes = FromInt<FileAttributes>(fileInfo.dwFileAttributes);
		m_Data.m_ReparsePointTags = ReparsePointTags::None;
		m_Data.m_CreationTime = fileInfo.ftCreationTime;
		m_Data.m_LastAccessTime = fileInfo.ftLastAccessTime;
		m_Data.m_ModificationTime = fileInfo.ftLastWriteTime;

		if (IsFile())
		{
			m_NativeData.nFileSizeLow = fileInfo.nFileSizeLow;
			m_NativeData.nFileSizeHigh = fileInfo.nFileSizeHigh;
		}
		else
		{
			m_Data.SetFileSize(0);
		}
	}
	void KxFileItem::FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& fileInfo) noexcept
	{
		m_Data.m_Attributes = FromInt<FileAttributes>(fileInfo.FileAttributes);
		m_Data.m_ReparsePointTags = ReparsePointTags::None;
		m_Data.m_CreationTime = FileTimeFromLARGE_INTEGER(fileInfo.CreationTime);
		m_Data.m_LastAccessTime = FileTimeFromLARGE_INTEGER(fileInfo.LastAccessTime);
		m_Data.m_ModificationTime = FileTimeFromLARGE_INTEGER(fileInfo.LastWriteTime);
	}
}
