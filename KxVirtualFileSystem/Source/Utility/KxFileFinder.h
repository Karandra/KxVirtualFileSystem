/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "Utility.h"

namespace KxVFS
{
	class KxFileItem;

	class KxFileFinder
	{
		public:
			static bool IsDirectoryEmpty(KxDynamicStringRefW directoryPath);

		private:
			const KxDynamicStringW m_Source;
			const KxDynamicStringW m_Filter;
			bool m_IsCanceled = false;

			HANDLE m_Handle = INVALID_HANDLE_VALUE;
			WIN32_FIND_DATAW m_FindData = {0};

		private:
			bool OnFound(const WIN32_FIND_DATAW& fileInfo);
			KxDynamicStringW ConstructSearchQuery() const;

		protected:
			virtual bool OnFound(const KxFileItem& foundItem);

		public:
			KxFileFinder(KxDynamicStringRefW source, KxDynamicStringRefW filter = {});
			virtual ~KxFileFinder();

		public:
			bool IsOK() const;
			bool IsCanceled() const
			{
				return m_IsCanceled;
			}
			bool Run();

			KxFileItem FindNext();
			void NotifyFound(const KxFileItem& foundItem)
			{
				m_IsCanceled = !OnFound(foundItem);
			}

			KxDynamicStringRefW GetSource() const
			{
				return m_Source;
			}
	};
}

namespace KxVFS
{
	class KxFileItem final
	{
		friend class KxFileFinder;

		private:
			KxDynamicStringW m_Source;
			KxDynamicStringW m_Name;
			uint32_t m_Attributes = INVALID_FILE_ATTRIBUTES;
			uint32_t m_ReparsePointAttributes = 0;
			FILETIME m_CreationTime = {0, 0};
			FILETIME m_LastAccessTime = {0, 0};
			FILETIME m_ModificationTime = {0, 0};
			int64_t m_FileSize = -1;

		private:
			void MakeNull(bool attribuesOnly = false);
			void Set(const WIN32_FIND_DATAW& fileInfo);

		public:
			KxFileItem() = default;
			KxFileItem(KxDynamicStringRefW fullPath);
			KxFileItem(const KxFileItem& other) = default;

		private:
			KxFileItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo);

		public:
			bool IsOK() const
			{
				return !m_Source.empty() && m_Attributes != INVALID_FILE_ATTRIBUTES;
			}

			bool IsNormalItem() const
			{
				return IsOK() && !IsCurrentOrParent() && !IsReparsePoint();
			}
			bool IsCurrentOrParent() const;
			bool IsDirectory() const
			{
				return m_Attributes & FILE_ATTRIBUTE_DIRECTORY;
			}
			bool IsFile() const
			{
				return !IsDirectory();
			}
			bool IsReparsePoint() const
			{
				return m_Attributes & FILE_ATTRIBUTE_REPARSE_POINT;
			}

			uint32_t GetAttributes() const
			{
				return m_Attributes;
			}
			uint32_t GetReparsePointAttributes() const
			{
				return m_ReparsePointAttributes;
			}
			FILETIME GetCreationTime() const
			{
				return m_CreationTime;
			}
			FILETIME GetLastAccessTime() const
			{
				return m_LastAccessTime;
			}
			FILETIME GetModificationTime() const
			{
				return m_ModificationTime;
			}
			int64_t GetFileSize() const
			{
				return m_FileSize;
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
			
			KxDynamicStringW GetFullPath() const
			{
				KxDynamicStringW out(m_Source);
				out += TEXT('\\');
				out += m_Name;

				return out;
			}

			bool UpdateInfo();
			WIN32_FIND_DATAW GetAsWIN32_FIND_DATA() const;
	};
}
