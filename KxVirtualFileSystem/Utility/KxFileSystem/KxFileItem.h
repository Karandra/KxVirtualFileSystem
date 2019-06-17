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
}

namespace KxVFS
{
	class KxVFS_API KxFileItemBase
	{
		public:
			using TShortName = KxBasicDynamicString<wchar_t, ARRAYSIZE(WIN32_FIND_DATAW::cAlternateFileName)>;

		public:
			void ExtractSourceAndName(KxDynamicStringRefW fullPath, KxDynamicStringW& source, KxDynamicStringW& name);
			static KxFileItemBase FromPath(KxDynamicStringRefW fullPath);

		protected:
			KxDynamicStringW m_Name;
			TShortName m_ShortName;
			FileAttributes m_Attributes = FileAttributes::Invalid;
			ReparsePointTags m_ReparsePointTags = ReparsePointTags::None;
			FILETIME m_CreationTime = {};
			FILETIME m_LastAccessTime = {};
			FILETIME m_ModificationTime = {};
			int64_t m_FileSize = -1;

		protected:
			FILETIME FileTimeFromLARGE_INTEGER(const LARGE_INTEGER& value) const
			{
				return *reinterpret_cast<const FILETIME*>(&value);
			}
			KxDynamicStringRefW TrimNamespace(KxDynamicStringRefW path) const;

			virtual void OnChange() { }

		public:
			KxFileItemBase() = default;
			KxFileItemBase(KxDynamicStringRefW fileName)
				:m_Name(fileName)
			{
			}
			virtual ~KxFileItemBase() = default;

		public:
			bool IsOK() const
			{
				return m_Attributes != FileAttributes::Invalid;
			}
			virtual void MakeNull(bool attribuesOnly = false);
			bool UpdateInfo(KxDynamicStringRefW fullPath, bool queryShortName = false);

			bool IsNormalItem() const
			{
				return IsOK() && !IsReparsePoint() && !IsCurrentOrParent();
			}
			bool IsCurrentOrParent() const;
			bool IsReparsePoint() const
			{
				return m_Attributes & FileAttributes::ReparsePoint;
			}

			bool IsDirectory() const
			{
				return m_Attributes & FileAttributes::Directory;
			}
			KxFileItemBase& SetDirectory()
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::Directory, true);
				OnChange();
				return *this;
			}

			bool IsFile() const
			{
				return !IsDirectory();
			}
			KxFileItemBase& SetFile()
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::Directory, false);
				OnChange();
				return *this;
			}

			bool IsReadOnly() const
			{
				return m_Attributes & FileAttributes::ReadOnly;
			}
			KxFileItemBase& SetReadOnly(bool value = true)
			{
				Utility::ModFlagRef(m_Attributes, FileAttributes::ReadOnly, value);
				OnChange();
				return *this;
			}

			FileAttributes GetAttributes() const
			{
				return m_Attributes;
			}
			void SetAttributes(FileAttributes attributes)
			{
				m_Attributes = attributes;
				OnChange();
			}
			void SetNormalAttributes()
			{
				m_Attributes = FileAttributes::Normal;
				OnChange();
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
					OnChange();
				}
			}

			FILETIME GetCreationTime() const
			{
				return m_CreationTime;
			}
			void SetCreationTime(const FILETIME& value)
			{
				m_CreationTime = value;
				OnChange();
			}
			void SetCreationTime(const LARGE_INTEGER& value)
			{
				m_CreationTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
			}
			
			FILETIME GetLastAccessTime() const
			{
				return m_LastAccessTime;
			}
			void SetLastAccessTime(const FILETIME& value)
			{
				m_LastAccessTime = value;
				OnChange();
			}
			void SetLastAccessTime(const LARGE_INTEGER& value)
			{
				m_LastAccessTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
			}

			FILETIME GetModificationTime() const
			{
				return m_ModificationTime;
			}
			void SetModificationTime(const FILETIME& value)
			{
				m_ModificationTime = value;
				OnChange();
			}
			void SetModificationTime(const LARGE_INTEGER& value)
			{
				m_ModificationTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
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
					OnChange();
				}
			}

			KxDynamicStringRefW GetName() const
			{
				return m_Name;
			}
			void SetName(KxDynamicStringRefW name)
			{
				m_Name = name;
				OnChange();
			}
			
			KxDynamicStringRefW GetShortName() const
			{
				return m_ShortName;
			}
			void SetShortName(KxDynamicStringRefW name)
			{
				m_ShortName = name;
				OnChange();
			}

			KxDynamicStringW GetFileExtension() const;
			void SetFileExtension(KxDynamicStringRefW ext);

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

namespace KxVFS
{
	class KxVFS_API KxFileItem: public KxFileItemBase
	{
		friend class KxFileFinder;

		private:
			KxDynamicStringW m_Source;

		public:
			KxFileItem() = default;
			KxFileItem(KxDynamicStringRefW fullPath)
			{
				SetFullPath(fullPath);
				KxFileItemBase::UpdateInfo(fullPath);
			}
			KxFileItem(KxDynamicStringRefW source, KxDynamicStringRefW fileName)
				:KxFileItemBase(fileName), m_Source(TrimNamespace(source))
			{
				UpdateInfo();
			}
			
		protected:
			KxFileItem(const KxFileFinder& finder, const WIN32_FIND_DATAW& fileInfo);

		public:
			KxFileItem& SetFile()
			{
				KxFileItemBase::SetFile();
				return *this;
			}
			KxFileItem& SetDirectory()
			{
				KxFileItemBase::SetDirectory();
				return *this;
			}
			KxFileItem& SetReadOnly(bool value = true)
			{
				KxFileItemBase::SetReadOnly(value);
				return *this;
			}

		public:
			void MakeNull(bool attribuesOnly) override;
			bool UpdateInfo(bool queryShortName = false);
			bool IsDirectoryEmpty() const;

			KxDynamicStringRefW GetSource() const
			{
				return m_Source;
			}
			void SetSource(KxDynamicStringRefW source)
			{
				m_Source = TrimNamespace(source);
				OnChange();
			}

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
				ExtractSourceAndName(fullPath, m_Source, m_Name);
				OnChange();
			}
	};
}
 