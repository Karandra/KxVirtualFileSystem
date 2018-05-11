#pragma once
#include "KxVirtualFileSystem.h"
#include "KxDynamicString.h"
class KxFileFinderItem;

class KxFileFinder
{
	public:
		static bool IsDirectoryEmpty(const KxDynamicStringRef& sDirectoryPath);

	private:
		const KxDynamicString m_Source;
		const KxDynamicString m_Filter;
		bool m_Canceled = false;

		HANDLE m_Handle = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATAW m_FindData = {0};

	private:
		bool OnFound(const WIN32_FIND_DATAW& tFileInfo);
		KxDynamicString ConstructSearchQuery() const;

	protected:
		virtual bool OnFound(const KxFileFinderItem& tFoundItem);

	public:
		KxFileFinder(const KxDynamicString& sSource, const KxDynamicStringRef& sFilter = KxNullDynamicStrigRef);
		virtual ~KxFileFinder();

	public:
		bool IsOK() const;
		bool IsCanceled() const
		{
			return m_Canceled;
		}
		bool Run();
		KxFileFinderItem FindNext();
		void NotifyFound(const KxFileFinderItem& tFoundItem)
		{
			m_Canceled = !OnFound(tFoundItem);
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
		void MakeNull(bool bAttribuesOnly = false);
		void Set(const WIN32_FIND_DATAW& tFileInfo);

	public:
		KxFileFinderItem() {}
		KxFileFinderItem(const KxDynamicStringRef& sFullPath);
		KxFileFinderItem(const KxFileFinderItem& other) = default;
		~KxFileFinderItem();

	private:
		KxFileFinderItem(KxFileFinder* pFinder, const WIN32_FIND_DATAW& tFileInfo);

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
		void SetSource(const KxDynamicStringRef& sSource)
		{
			m_Source = sSource;
		}
		const KxDynamicString& GetName() const
		{
			return m_Name;
		}
		void SetName(const KxDynamicStringRef& sName)
		{
			m_Name = sName;
		}
		KxDynamicString GetFullPath() const
		{
			KxDynamicString sOut(m_Source);
			sOut += TEXT('\\');
			sOut += m_Name;

			return sOut;
		}

		bool UpdateInfo();
		WIN32_FIND_DATAW GetAsWIN32_FIND_DATA() const;
};