#pragma once
#include "KxVirtualFileSystem.h"
#include <string_view>

class KxVFSUtility
{
	public:
		typedef std::unordered_set<size_t> StringSearcherHash;

	public:
		static std::wstring ToUTF16(const char* text);
		static std::string ToUTF8(const WCHAR* text);

		static WORD LocaleIDToLangID(WORD localeID);
		static std::wstring FormatMessage(DWORD flags, const void* source, DWORD messageID, WORD langID = 0);
		static std::wstring GetErrorMessage(DWORD code = GetLastError(), WORD langID = 0);
		template<class T, class V> static bool SetIfNotNull(T* p, const V& v)
		{
			if (p)
			{
				*p = v;
				return true;
			}
			return false;
		}

		static bool CreateFolderTree(const WCHAR* pathW, bool noLast = false, SECURITY_ATTRIBUTES* securityAttributes = NULL, DWORD* errorCodeOut = NULL);
		static bool IsFolderEmpty(const WCHAR* path);
		static std::wstring GetDriveFromPath(const WCHAR* path);
		static DWORD GetFileAttributes(const WCHAR* path);
		static bool IsExist(const WCHAR* path);
		static bool IsFileExist(const WCHAR* path);
		static bool IsFolderExist(const WCHAR* path);

		static size_t HashString(const std::wstring_view& string)
		{
			return std::hash<std::wstring_view>()(string);
		}

		static int DebugPrint(const WCHAR* fmt, ...);
};

#ifdef _DEBUG
	#define KxVFSDebugPrint KxVFSUtility::DebugPrint
#else
	#define KxVFSDebugPrint
#endif // _DEBUG
