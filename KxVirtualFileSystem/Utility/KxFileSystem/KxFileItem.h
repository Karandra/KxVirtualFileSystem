/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility
{
	class KxVFS_API KxFileFinder;

	class KxVFS_API KxFileItem
	{
		friend class KxFileFinder;

		private:
			KxDynamicStringW m_Name;
			KxDynamicStringW m_Source;
			uint32_t m_Attributes = INVALID_FILE_ATTRIBUTES;
			uint32_t m_ReparsePointAttributes = 0;
			FILETIME m_CreationTime;
			FILETIME m_LastAccessTime;
			FILETIME m_ModificationTime;
			int64_t m_FileSize = -1;

		private:
			void MakeNull(bool attribuesOnly = false);
			void Set(const WIN32_FIND_DATAW& fileInfo);
			bool DoUpdateInfo(KxDynamicStringRefW fullPath);

		public:
			KxFileItem() = default;
			KxFileItem(KxDynamicStringRefW fullPath);
			KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName);

		private:
			KxFileItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo);

		public:
			bool IsOK() const
			{
				return m_Attributes != INVALID_FILE_ATTRIBUTES;
			}
			bool UpdateInfo()
			{
				return DoUpdateInfo(GetFullPath());
			}

			bool IsNormalItem() const
			{
				return IsOK() && !IsReparsePoint() && !IsCurrentOrParent();
			}
			bool IsCurrentOrParent() const;
			bool IsReparsePoint() const
			{
				return m_Attributes & FILE_ATTRIBUTE_REPARSE_POINT;
			}

			bool IsDirectory() const
			{
				return m_Attributes & FILE_ATTRIBUTE_DIRECTORY;
			}
			bool IsDirectoryEmpty() const;
			KxFileItem& SetDirectory()
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_DIRECTORY, true);
				return *this;
			}

			bool IsFile() const
			{
				return !IsDirectory();
			}
			KxFileItem& SetFile()
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_DIRECTORY, false);
				return *this;
			}

			bool IsReadOnly() const
			{
				return m_Attributes & FILE_ATTRIBUTE_READONLY;
			}
			KxFileItem& SetReadOnly(bool value = true)
			{
				Utility::ModFlagRef(m_Attributes, FILE_ATTRIBUTE_READONLY, value);
				return *this;
			}

			uint32_t GetAttributes() const
			{
				return m_Attributes;
			}
			uint32_t GetReparsePointAttributes() const
			{
				return m_ReparsePointAttributes;
			}
			void SetNormalAttributes()
			{
				m_Attributes = FILE_ATTRIBUTE_NORMAL;
			}

			FILETIME GetCreationTime() const
			{
				return m_CreationTime;
			}
			void SetCreationTime(const FILETIME& value)
			{
				m_CreationTime = value;
			}
		
			FILETIME GetLastAccessTime() const
			{
				return m_LastAccessTime;
			}
			void SetLastAccessTime(const FILETIME& value)
			{
				m_LastAccessTime = value;
			}
		
			FILETIME GetModificationTime() const
			{
				return m_ModificationTime;
			}
			void SetModificationTime(const FILETIME& value)
			{
				m_ModificationTime = value;
			}

			int64_t GetFileSize() const
			{
				return m_FileSize;
			}
			void SetFileSize(int64_t size)
			{
				m_FileSize = size;
			}

			KxDynamicStringRefW GetSource() const
			{
				return m_Source;
			}
			void SetSource(KxDynamicStringRefW source)
			{
				m_Source = source;
			}
		
			KxDynamicStringRefW GetName() const
			{
				return m_Name;
			}
			void SetName(KxDynamicStringRefW name)
			{
				m_Name = name;
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
				KxDynamicStringW fullPath = Utility::LongPathPrefix;
				fullPath += m_Source;
				fullPath += L'\\';
				fullPath += m_Name;
				return fullPath;
			}
			void SetFullPath(KxDynamicStringRefW fullPath)
			{
				m_Source = KxDynamicStringW(fullPath).before_last(L'\\', &m_Name);
			}
			
			WIN32_FIND_DATAW ToWIN32_FIND_DATA() const;
	};
}
