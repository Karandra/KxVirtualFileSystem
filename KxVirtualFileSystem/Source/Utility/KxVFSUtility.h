#pragma once
#include "KxVirtualFileSystem.h"
#include <string_view>

class KxVFSUtility
{
	public:
		typedef std::unordered_set<size_t> StringSearcherHash;

	public:
		static std::wstring ToUTF16(const char* sText);
		static std::string ToUTF8(const WCHAR* sText);

		static WORD LocaleIDToLangID(WORD nLocaleID);
		static std::wstring FormatMessage(DWORD nFlags, const void* pSource, DWORD nMessageID, WORD nLangID = 0);
		static std::wstring GetErrorMessage(DWORD nCode = GetLastError(), WORD nLangID = 0);
		template<class T, class V> static bool SetIfNotNull(T* pData, const V& vData)
		{
			if (pData)
			{
				*pData = vData;
				return true;
			}
			return false;
		}

		static bool CreateFolderTree(const WCHAR* sPathW, bool bNoLast = false, SECURITY_ATTRIBUTES* pSecurityAttributes = NULL, DWORD* pErrorCode = NULL);
		static bool IsFolderEmpty(const WCHAR* sPath);
		static std::wstring GetDriveFromPath(const WCHAR* sPath);
		static DWORD GetFileAttributes(const WCHAR* sPath);
		static bool IsExist(const WCHAR* sPath);
		static bool IsFileExist(const WCHAR* sPath);
		static bool IsFolderExist(const WCHAR* sPath);

		static size_t HashString(const std::wstring_view& sString)
		{
			return std::hash<std::wstring_view>()(sString);
		}

		static int DebugPrint(const WCHAR* fmt, ...);
};

#ifdef _DEBUG
	#define KxVFSDebugPrint KxVFSUtility::DebugPrint
#else
	#define KxVFSDebugPrint
#endif // _DEBUG
