/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeDokan.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API KxFileFinder;

	class KxVFS_API KxFileItem
	{
		friend class KxFileFinder;

		public:
			using TShortName = KxBasicDynamicString<wchar_t, ARRAYSIZE(WIN32_FIND_DATAW::cAlternateFileName)>;

		private:
			KxDynamicStringW m_Name;
			KxDynamicStringW m_Source;
			TShortName m_ShortName;
			FileAttributes m_Attributes = FileAttributes::Invalid;
			ReparsePointTags m_ReparsePointTags = ReparsePointTags::None;
			FILETIME m_CreationTime;
			FILETIME m_LastAccessTime;
			FILETIME m_ModificationTime;
			int64_t m_FileSize = -1;

		private:
			void MakeNull(bool attribuesOnly = false);
			FILETIME FileTimeFromLARGE_INTEGER(const LARGE_INTEGER& value) const
			{
				return *reinterpret_cast<const FILETIME*>(&value);
			}
			bool DoUpdateInfo(KxDynamicStringRefW fullPath, bool queryShortName = false);
			KxDynamicStringRefW TrimNamespace(KxDynamicStringRefW path) const;

		public:
			KxFileItem() = default;
			KxFileItem(KxDynamicStringRefW fullPath);
			KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName);

		private:
			KxFileItem(const KxFileFinder& finder, const WIN32_FIND_DATAW& fileInfo);

		public:
			bool IsOK() const
			{
				return m_Attributes != FileAttributes::Invalid;
			}
			bool UpdateInfo(bool queryShortName = false)
			{
				return DoUpdateInfo(GetFullPath(), queryShortName);
			}

			bool IsNormalItem() const
			{
				return IsOK() && !IsReparsePoint() && !IsCurrentOrParent();
			}
			bool IsCurrentOrParent() const;
			bool IsReparsePoint() const
			{
				return ToBool(m_Attributes & FileAttributes::ReparsePoint);
			}

			bool IsDirectory() const
			{
				return ToBool(m_Attributes & FileAttributes::Directory);
			}
			bool IsDirectoryEmpty() const;
			KxFileItem& SetDirectory()
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::Directory, true);
				return *this;
			}

			bool IsFile() const
			{
				return !IsDirectory();
			}
			KxFileItem& SetFile()
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::Directory, false);
				return *this;
			}

			bool IsReadOnly() const
			{
				return ToBool(m_Attributes & FileAttributes::ReadOnly);
			}
			KxFileItem& SetReadOnly(bool value = true)
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::ReadOnly, value);
				return *this;
			}

			FileAttributes GetAttributes() const
			{
				return m_Attributes;
			}
			void SetAttributes(FileAttributes attributes)
			{
				m_Attributes = attributes;
			}
			void SetNormalAttributes()
			{
				m_Attributes = FileAttributes::Normal;
			}
			
			ReparsePointTags GetReparsePointTags() const
			{
				return m_ReparsePointTags;
			}
			void SetReparsePointTags(ReparsePointTags tags)
			{
				if (IsReparsePoint())
				{
					m_ReparsePointTags = tags;
				}
			}

			FILETIME GetCreationTime() const
			{
				return m_CreationTime;
			}
			void SetCreationTime(const FILETIME& value)
			{
				m_CreationTime = value;
			}
			void SetCreationTime(const LARGE_INTEGER& value)
			{
				m_CreationTime = FileTimeFromLARGE_INTEGER(value);
			}
			
			FILETIME GetLastAccessTime() const
			{
				return m_LastAccessTime;
			}
			void SetLastAccessTime(const FILETIME& value)
			{
				m_LastAccessTime = value;
			}
			void SetLastAccessTime(const LARGE_INTEGER& value)
			{
				m_LastAccessTime = FileTimeFromLARGE_INTEGER(value);
			}

			FILETIME GetModificationTime() const
			{
				return m_ModificationTime;
			}
			void SetModificationTime(const FILETIME& value)
			{
				m_ModificationTime = value;
			}
			void SetModificationTime(const LARGE_INTEGER& value)
			{
				m_ModificationTime = FileTimeFromLARGE_INTEGER(value);
			}

			int64_t GetFileSize() const
			{
				return m_FileSize;
			}
			void SetFileSize(int64_t size)
			{
				if (IsFile())
				{
					m_FileSize = size;
				}
			}

			KxDynamicStringRefW GetSource() const
			{
				return m_Source;
			}
			void SetSource(KxDynamicStringRefW source)
			{
				m_Source = TrimNamespace(source);
			}
			
			KxDynamicStringRefW GetName() const
			{
				return m_Name;
			}
			void SetName(KxDynamicStringRefW name)
			{
				m_Name = name;
			}
			
			KxDynamicStringRefW GetShortName() const
			{
				return m_ShortName;
			}
			void SetShortName(KxDynamicStringRefW name)
			{
				m_ShortName = name;
			}

			KxDynamicStringW GetFileExtension() const;
			void SetFileExtension(KxDynamicStringRefW ext);

			KxDynamicStringW GetFullPath() const
			{
				KxDynamicStringW fullPath = m_Source;
				fullPath += L'\\';
				fullPath += m_Name;
				return fullPath;
			}
			KxDynamicStringW GetFullPathWithNS() const
			{
				KxDynamicStringW fullPath = Utility::GetLongPathPrefix();
				fullPath += m_Source;
				fullPath += L'\\';
				fullPath += m_Name;
				return fullPath;
			}
			void SetFullPath(KxDynamicStringRefW fullPath)
			{
				KxDynamicStringW source = KxDynamicStringW(fullPath).before_last(L'\\', &m_Name);
				m_Source = TrimNamespace(source);
			}
			
		public:
			void FromWIN32_FIND_DATA(const WIN32_FIND_DATAW& findInfo);
			void ToWIN32_FIND_DATA(WIN32_FIND_DATAW& findData) const;
			WIN32_FIND_DATAW ToWIN32_FIND_DATA() const
			{
				WIN32_FIND_DATAW findData = {0};
				ToWIN32_FIND_DATA(findData);
				return findData;
			}
	
			void ToBY_HANDLE_FILE_INFORMATION(BY_HANDLE_FILE_INFORMATION& fileInfo) const;
			BY_HANDLE_FILE_INFORMATION ToBY_HANDLE_FILE_INFORMATION() const
			{
				BY_HANDLE_FILE_INFORMATION fileInfo = {};
				ToBY_HANDLE_FILE_INFORMATION(fileInfo);
				return fileInfo;
			}
			void FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& fileInfo);
			void FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& basicInfo);
	};
}
