#include "KxVirtualFileSystem.h"
#include "KxVFSUtility.h"
#include "KxFileFinder.h"

std::wstring KxVFSUtility::ToUTF16(const char* sText)
{
	//return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>().from_bytes(sText);

	std::wstring sOut;
	int iReqLength = MultiByteToWideChar(CP_UTF8, 0, sText, -1, NULL, 0);
	if (iReqLength != 0)
	{
		sOut.resize((size_t)iReqLength);
		MultiByteToWideChar(CP_UTF8, 0, sText, -1, sOut.data(), iReqLength);
	}
	return sOut;
}
std::string KxVFSUtility::ToUTF8(const WCHAR* sText)
{
	//return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>().to_bytes(sText);

	std::string sOut;
	int iReqLength = WideCharToMultiByte(CP_UTF8, 0, sText, -1, NULL, 0, NULL, NULL);
	if (iReqLength != 0)
	{
		sOut.resize((size_t)iReqLength);
		WideCharToMultiByte(CP_UTF8, 0, sText, -1, sOut.data(), iReqLength, NULL, NULL);
	}
	return sOut;
}

WORD KxVFSUtility::LocaleIDToLangID(WORD nLocaleID)
{
	return MAKELANGID(PRIMARYLANGID(nLocaleID), SUBLANGID(nLocaleID));
}
std::wstring KxVFSUtility::FormatMessage(DWORD nFlags, const void* pSource, DWORD nMessageID, WORD nLangID)
{
	LPWSTR pFormattedMessage = NULL;
	DWORD nFormattedMessageLength = ::FormatMessageW(nFlags|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS, pSource, nMessageID, LocaleIDToLangID(nLangID), (LPWSTR)&pFormattedMessage, 0, NULL);
	if (nFormattedMessageLength != 0 && pFormattedMessage != NULL)
	{
		std::wstring sFormattedMessage(pFormattedMessage, nFormattedMessageLength);
		::LocalFree(pFormattedMessage);
		return sFormattedMessage;
	}
	return L"";
}
std::wstring KxVFSUtility::GetErrorMessage(DWORD nCode, WORD nLangID)
{
	std::wstring sMessage = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, nCode, nLangID);
	return L"[" + std::to_wstring(nCode) + L"] " + sMessage;
}

std::wstring KxVFSUtility::GetDriveFromPath(const WCHAR* sPath)
{
	if (sPath)
	{
		std::wstring_view sPathView(sPath);
		size_t nIndex = sPathView.find(L':');
		if (nIndex != std::wstring_view::npos && nIndex >= 1)
		{
			WCHAR sVolumeRoot[4];
			sVolumeRoot[0] = sPath[nIndex - 1];
			sVolumeRoot[1] = L':';
			sVolumeRoot[2] = L'\\';
			sVolumeRoot[3] = L'\0';
			return sVolumeRoot;
		}
	}
	return L"";
}
DWORD KxVFSUtility::GetFileAttributes(const WCHAR* sPath)
{
	return ::GetFileAttributesW(sPath);
}
bool KxVFSUtility::IsExist(const WCHAR* sPath)
{
	return ::GetFileAttributesW(sPath) != INVALID_FILE_ATTRIBUTES;
}
bool KxVFSUtility::IsFileExist(const WCHAR* sPath)
{
	DWORD nAttributes = GetFileAttributesW(sPath);
	return (nAttributes != INVALID_FILE_ATTRIBUTES) && !(nAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
bool KxVFSUtility::IsFolderExist(const WCHAR* sPath)
{
	DWORD nAttributes = GetFileAttributesW(sPath);
	return (nAttributes != INVALID_FILE_ATTRIBUTES) && (nAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool KxVFSUtility::CreateFolderTree(const WCHAR* sPathW, bool bNoLast, SECURITY_ATTRIBUTES* pSecurityAttributes, DWORD* pErrorCode)
{
	if (!IsFolderExist(sPathW))
	{
		std::wstring sPath(sPathW);
		std::vector<std::wstring> tFolderArray;

		// Construct array
		const auto stdNOT_FOUND = std::wstring::npos;
		size_t nFolderStart = sPath.find(':');
		if (nFolderStart != stdNOT_FOUND)
		{
			if (sPath[nFolderStart+1] == '\\')
			{
				nFolderStart += 2;
			}
		}
		else
		{
			nFolderStart = 0;
		}

		size_t nNext = stdNOT_FOUND;
		do
		{
			nNext = sPath.find('\\', nFolderStart);
			tFolderArray.push_back(sPath.substr(nFolderStart, nNext - nFolderStart));
			nFolderStart = nNext + 1;
		}
		while (nNext != stdNOT_FOUND);

		// Create folder
		BOOL bRet = FALSE;
		std::wstring sFullPath = GetDriveFromPath(sPathW) + L'\\';
		for (size_t i = 0; i < tFolderArray.size(); i++)
		{
			sFullPath.append(tFolderArray[i]).append(L"\\");
			bRet = ::CreateDirectoryW(sFullPath.c_str(), pSecurityAttributes);
			if (!bRet)
			{
				SetIfNotNull(pErrorCode, GetLastError());
			}

			if (bNoLast && i + 2 == tFolderArray.size())
			{
				break;
			}
		}
		return IsFolderExist(sPathW);
	}

	SetIfNotNull(pErrorCode, 0);
	return true;
}
bool KxVFSUtility::IsFolderEmpty(const WCHAR* sPath)
{
	return KxFileFinder::IsDirectoryEmpty(sPath);
}

int KxVFSUtility::DebugPrint(const WCHAR* fmt, ...)
{
	WCHAR sBuffer[INT16_MAX] = {0};

	va_list argptr;
	va_start(argptr, fmt);
	int nCount = vswprintf(sBuffer, ARRAYSIZE(sBuffer), fmt, argptr);
	va_end(argptr);

	::OutputDebugStringW(sBuffer);
	return nCount;
}
