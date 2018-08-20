/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxDynamicString.h"
class KxFileFinderItem;

class KxFileFinder
{
	public:
		static bool IsDirectoryEmpty(const KxDynamicStringRef& directoryPath);

	private:
		const KxDynamicString m_Source;
		const KxDynamicString m_Filter;
		bool m_Canceled = false;

		HANDLE m_Handle = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATAW m_FindData = {0};

	private:
		bool OnFound(const WIN32_FIND_DATAW& fileInfo);
		KxDynamicString ConstructSearchQuery() const;

	protected:
		virtual bool OnFound(const KxFileFinderItem& foundItem);

	public:
		KxFileFinder(const KxDynamicString& source, const KxDynamicStringRef& filter = KxNullDynamicStrigRef);
		virtual ~KxFileFinder();

	public:
		bool IsOK() const;
		bool IsCanceled() const
		{
			return m_Canceled;
		}
		bool Run();
		KxFileFinderItem FindNext();
		void NotifyFound(const KxFileFinderItem& foundItem)
		{
			m_Canceled = !OnFound(foundItem);
		}

		const KxDynamicString& GetSource() const
		{
			return m_Source;
		}
};

class KxFileFinderItem
{
	friend class KxFileFinder;

	private:
		KxDynamicString m_Source;
		KxDynamicString m_Name;
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
		KxFileFinderItem() {}
		KxFileFinderItem(const KxDynamicStringRef& fullPath);
		KxFileFinderItem(const KxFileFinderItem& other) = default;
		~KxFileFinderItem();

	private:
		KxFileFinderItem(KxFileFinder* finder, const WIN32_FIND_DATAW& fileInfo);

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
		
		const KxDynamicString& GetSource() const
		{
			return m_Source;
		}
		void SetSource(const KxDynamicStringRef& source)
		{
			m_Source = source;
		}
		const KxDynamicString& GetName() const
		{
			return m_Name;
		}
		void SetName(const KxDynamicStringRef& name)
		{
			m_Name = name;
		}
		KxDynamicString GetFullPath() const
		{
			KxDynamicString out(m_Source);
			out += TEXT('\\');
			out += m_Name;

			return out;
		}

		bool UpdateInfo();
		WIN32_FIND_DATAW GetAsWIN32_FIND_DATA() const;
};