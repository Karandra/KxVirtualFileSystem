#include "stdafx.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileItem.h"
#include "FileFinder.h"

namespace KxVFS
{
	void FileItem::ExtractSourceAndName(DynamicStringRefW fullPath, DynamicStringW& source, DynamicStringW& name)
	{
		source = DynamicStringW(fullPath).before_last(L'\\', &name);
		source = TrimNamespace(source);
	}
	DynamicStringRefW FileItem::TrimNamespace(DynamicStringRefW path) noexcept
	{
		const size_t prefixLength = Utility::GetLongPathPrefix().size();
		if (path.substr(0, prefixLength) == Utility::GetLongPathPrefix())
		{
			path.remove_prefix(prefixLength);
		}
		return path;
	}

	FileItem::FileItem(const FileFinder& finder, const Win32FindData& findData)
		:m_Data(findData), m_Source(TrimNamespace(finder.GetSource()))
	{
	}
	FileItem::FileItem(const FileFinder& finder, const WIN32_FIND_DATAW& findData)
		:m_NativeData(findData), m_Source(TrimNamespace(finder.GetSource()))
	{
	}

	void FileItem::MakeNull(bool attribuesOnly) noexcept
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
	bool FileItem::UpdateInfo(DynamicStringRefW fullPath, bool queryShortName)
	{
		Utility::CallAtScopeExit atExit([this]()
		{
			OnChange();
		});
		
		FileFinder finder(fullPath);
		finder.QueryShortNames(queryShortName);

		FileItem item = finder.FindNext();
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
	bool FileItem::IsDirectoryEmpty() const
	{
		return IsDirectory() && FileFinder::IsDirectoryEmpty(m_Source);
	}

	DynamicStringW FileItem::GetFileExtension() const noexcept
	{
		if (IsFile())
		{
			const DynamicStringRefW name = GetName();
			const size_t pos = name.rfind(L'.');
			if (pos != DynamicStringW::npos)
			{
				return name.substr(pos + 1);
			}
		}
		return {};
	}
	void FileItem::SetFileExtension(DynamicStringRefW ext)
	{
		if (IsFile())
		{
			DynamicStringW name = GetName();
			const size_t pos = name.rfind(L'.');
			if (pos != DynamicStringRefW::npos)
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

	void FileItem::ToBY_HANDLE_FILE_INFORMATION(BY_HANDLE_FILE_INFORMATION& fileInfo) const noexcept
	{
		fileInfo.dwFileAttributes = m_Data.m_Attributes.ToInt();
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
	void FileItem::FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& fileInfo) noexcept
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
	void FileItem::FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& fileInfo) noexcept
	{
		m_Data.m_Attributes = FromInt<FileAttributes>(fileInfo.FileAttributes);
		m_Data.m_ReparsePointTags = ReparsePointTags::None;
		m_Data.m_CreationTime = FileTimeFromLARGE_INTEGER(fileInfo.CreationTime);
		m_Data.m_LastAccessTime = FileTimeFromLARGE_INTEGER(fileInfo.LastAccessTime);
		m_Data.m_ModificationTime = FileTimeFromLARGE_INTEGER(fileInfo.LastWriteTime);
	}
}
