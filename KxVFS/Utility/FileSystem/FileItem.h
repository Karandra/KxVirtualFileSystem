#pragma once
#include "KxVFS/Common.hpp"
#include "KxVFS/Misc/IncludeDokan.h"
#include "KxVFS/Utility.h"

namespace KxVFS
{
	class KxVFS_API FileFinder;
}

namespace KxVFS
{
	struct alignas(WIN32_FIND_DATAW) Win32FindData final
	{
		private:
			template<class T>
			static void AssignName(T& name, DynamicStringRefW fileName) noexcept
			{
				const size_t length = std::min(std::size(name), fileName.length());
				Utility::CopyMemory(name, fileName.data(), length);
				name[length] = L'\0';
			}

		public:
			FlagSet<FileAttributes> m_Attributes = FileAttributes::Invalid;
			FILETIME m_CreationTime = {};
			FILETIME m_LastAccessTime = {};
			FILETIME m_ModificationTime = {};
			uint32_t m_FileSizeHigh = 0;
			uint32_t m_FileSizeLow = 0;
			FlagSet<ReparsePointTags> m_ReparsePointTags;
			uint32_t m_Reserved1 = 0;
			wchar_t m_Name[ARRAYSIZE(WIN32_FIND_DATAW::cFileName)];
			wchar_t m_ShortName[ARRAYSIZE(WIN32_FIND_DATAW::cAlternateFileName)];

		public:
			Win32FindData() noexcept = default;
			Win32FindData(DynamicStringRefW fileName) noexcept
			{
				static_assert(sizeof(Win32FindData) == sizeof(WIN32_FIND_DATAW));
				static_assert(alignof(Win32FindData) == alignof(WIN32_FIND_DATAW));
				static_assert(std::is_standard_layout_v<Win32FindData> && std::is_trivially_copyable_v<Win32FindData>);

				AssignName(m_Name, fileName);
				m_ShortName[0] = L'\0';
			}

		public:
			void SetName(DynamicStringRefW fileName) noexcept
			{
				AssignName(m_Name, fileName);
			}
			void SetShortName(DynamicStringRefW fileName) noexcept
			{
				AssignName(m_ShortName, fileName);
			}
			
			int64_t GetFileSize() const noexcept
			{
				return Utility::LowHighToInt64(m_FileSizeLow, m_FileSizeHigh);
			}
			void SetFileSize(int64_t size) noexcept
			{
				Utility::Int64ToLowHigh(size, m_FileSizeLow, m_FileSizeHigh);
			}
	};
}

namespace KxVFS
{
	class KxVFS_API FileItem final
	{
		friend class FileFinder;

		public:
			static void ExtractSourceAndName(DynamicStringRefW fullPath, DynamicStringW& source, DynamicStringW& name);

		protected:
			union
			{
				Win32FindData m_Data;
				WIN32_FIND_DATAW m_NativeData;
			};
			DynamicStringW m_Source;

		protected:
			static FILETIME FileTimeFromLARGE_INTEGER(const LARGE_INTEGER& value) noexcept
			{
				return *reinterpret_cast<const FILETIME*>(&value);
			}
			static DynamicStringRefW TrimNamespace(DynamicStringRefW path) noexcept;

			void OnChange()
			{
			}

		private:
			FileItem(const FileFinder& finder, const Win32FindData& findData);
			FileItem(const FileFinder& finder, const WIN32_FIND_DATAW& findData);

		public:
			FileItem() noexcept
				:m_Data()
			{
			}
			FileItem(const Win32FindData& findData) noexcept
				:m_Data(findData)
			{
			}
			FileItem(const WIN32_FIND_DATAW& findData) noexcept
				:m_NativeData(findData)
			{
			}
			FileItem(DynamicStringRefW fullPath)
				:FileItem()
			{
				SetFullPath(fullPath);
				UpdateInfo(fullPath);
			}
			FileItem(DynamicStringRefW path, DynamicStringRefW fileName) noexcept
				:m_Data(fileName), m_Source(path)
			{
				UpdateInfo();
			}

		public:
			bool IsOK() const noexcept
			{
				return m_Data.m_Attributes != FileAttributes::Invalid;
			}
			void MakeNull(bool attribuesOnly = false) noexcept;
			bool UpdateInfo(DynamicStringRefW fullPath, bool queryShortName = false);
			bool UpdateInfo(bool queryShortName = false)
			{
				const DynamicStringW fullPath = GetFullPath();
				return UpdateInfo(fullPath, queryShortName);
			}
			bool IsDirectoryEmpty() const;

			bool IsNormalItem() const noexcept
			{
				return IsOK() && !IsReparsePoint() && !IsCurrentOrParent();
			}
			bool IsCurrentOrParent() const noexcept
			{
				constexpr DynamicStringRefW dot = L".";
				constexpr DynamicStringRefW dotDot = L"..";

				const DynamicStringRefW name = GetName();
				return name == dot || name == dotDot;
			}
			bool IsReparsePoint() const noexcept
			{
				return m_Data.m_Attributes & FileAttributes::ReparsePoint;
			}

			bool IsDirectory() const noexcept
			{
				return m_Data.m_Attributes & FileAttributes::Directory;
			}
			FileItem& SetDirectory() noexcept
			{
				Utility::ModFlagRef(m_Data.m_Attributes, FileAttributes::Directory, true);
				OnChange();
				return *this;
			}

			bool IsFile() const noexcept
			{
				return !IsDirectory();
			}
			FileItem& SetFile() noexcept
			{
				Utility::ModFlagRef(m_Data.m_Attributes, FileAttributes::Directory, false);
				OnChange();
				return *this;
			}

			bool IsReadOnly() const noexcept
			{
				return m_Data.m_Attributes & FileAttributes::ReadOnly;
			}
			FileItem& SetReadOnly(bool value = true) noexcept
			{
				Utility::ModFlagRef(m_Data.m_Attributes, FileAttributes::ReadOnly, value);
				OnChange();
				return *this;
			}

			FlagSet<FileAttributes> GetAttributes() const noexcept
			{
				return m_Data.m_Attributes;
			}
			void SetAttributes(FlagSet<FileAttributes> attributes) noexcept
			{
				m_Data.m_Attributes = attributes;
				OnChange();
			}
			void SetNormalAttributes() noexcept
			{
				m_Data.m_Attributes = FileAttributes::Normal;
				OnChange();
			}
			
			FlagSet<ReparsePointTags> GetReparsePointTags() const noexcept
			{
				return m_Data.m_ReparsePointTags;
			}
			void SetReparsePointTags(FlagSet<ReparsePointTags> tags) noexcept
			{
				if (IsReparsePoint())
				{
					m_Data.m_ReparsePointTags = tags;
					OnChange();
				}
			}

			FILETIME GetCreationTime() const noexcept
			{
				return m_Data.m_CreationTime;
			}
			void SetCreationTime(const FILETIME& value) noexcept
			{
				m_Data.m_CreationTime = value;
				OnChange();
			}
			void SetCreationTime(const LARGE_INTEGER& value) noexcept
			{
				m_Data.m_CreationTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
			}
			
			FILETIME GetLastAccessTime() const noexcept
			{
				return m_Data.m_LastAccessTime;
			}
			void SetLastAccessTime(const FILETIME& value) noexcept
			{
				m_Data.m_LastAccessTime = value;
				OnChange();
			}
			void SetLastAccessTime(const LARGE_INTEGER& value) noexcept
			{
				m_Data.m_LastAccessTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
			}

			FILETIME GetModificationTime() const noexcept
			{
				return m_Data.m_ModificationTime;
			}
			void SetModificationTime(const FILETIME& value) noexcept
			{
				m_Data.m_ModificationTime = value;
				OnChange();
			}
			void SetModificationTime(const LARGE_INTEGER& value) noexcept
			{
				m_Data.m_ModificationTime = FileTimeFromLARGE_INTEGER(value);
				OnChange();
			}

			int64_t GetFileSize() const noexcept
			{
				return m_Data.GetFileSize();
			}
			void SetFileSize(int64_t size) noexcept
			{
				if (IsFile())
				{
					m_Data.SetFileSize(size);
					OnChange();
				}
			}

			DynamicStringRefW GetName() const noexcept
			{
				return m_Data.m_Name;
			}
			void SetName(DynamicStringRefW name) noexcept
			{
				m_Data.SetName(name);
				OnChange();
			}
			
			DynamicStringRefW GetShortName() const noexcept
			{
				return m_Data.m_ShortName;
			}
			void SetShortName(DynamicStringRefW name) noexcept
			{
				m_Data.SetShortName(name);
				OnChange();
			}

			DynamicStringW GetFileExtension() const noexcept;
			void SetFileExtension(DynamicStringRefW ext);

			DynamicStringRefW GetSource() const noexcept
			{
				return m_Source;
			}
			void SetSource(DynamicStringRefW source)
			{
				m_Source = TrimNamespace(source);
				OnChange();
			}

			DynamicStringW GetFullPath() const
			{
				DynamicStringW fullPath = m_Source;
				fullPath += L'\\';
				fullPath += m_Data.m_Name;
				return fullPath;
			}
			DynamicStringW GetFullPathWithNS() const
			{
				DynamicStringW fullPath = Utility::GetLongPathPrefix();
				fullPath += m_Source;
				fullPath += L'\\';
				fullPath += m_Data.m_Name;
				return fullPath;
			}
			void SetFullPath(DynamicStringRefW fullPath)
			{
				DynamicStringW name;
				ExtractSourceAndName(fullPath, m_Source, name);
				m_Data.SetName(name);

				OnChange();
			}

		public:
			void FromWIN32_FIND_DATA(const WIN32_FIND_DATAW& findInfo) noexcept
			{
				m_NativeData = findInfo;
			}
			const WIN32_FIND_DATAW& AsWIN32_FIND_DATA() const noexcept
			{
				return m_NativeData;
			}
			WIN32_FIND_DATAW& AsWIN32_FIND_DATA() noexcept
			{
				return m_NativeData;
			}
			
			void ToBY_HANDLE_FILE_INFORMATION(BY_HANDLE_FILE_INFORMATION& fileInfo) const noexcept;
			BY_HANDLE_FILE_INFORMATION ToBY_HANDLE_FILE_INFORMATION() const noexcept
			{
				BY_HANDLE_FILE_INFORMATION fileInfo = {};
				ToBY_HANDLE_FILE_INFORMATION(fileInfo);
				return fileInfo;
			}
			void FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& fileInfo) noexcept;
			void FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& basicInfo) noexcept;
	};
}
