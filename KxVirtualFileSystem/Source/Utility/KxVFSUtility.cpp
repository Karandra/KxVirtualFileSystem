/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxVFSUtility.h"
#include "KxFileFinder.h"

std::wstring KxVFSUtility::ToUTF16(const char* text)
{
	std::wstring out;
	int reqLength = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
	if (reqLength != 0)
	{
		out.resize((size_t)reqLength);
		MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), reqLength);
	}
	return out;
}
std::string KxVFSUtility::ToUTF8(const WCHAR* text)
{
	std::string out;
	int reqLength = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (reqLength != 0)
	{
		out.resize((size_t)reqLength);
		WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), reqLength, NULL, NULL);
	}
	return out;
}

WORD KxVFSUtility::LocaleIDToLangID(WORD localeID)
{
	return MAKELANGID(PRIMARYLANGID(localeID), SUBLANGID(localeID));
}
std::wstring KxVFSUtility::FormatMessage(DWORD flags, const void* source, DWORD messageID, WORD langID)
{
	LPWSTR formattedMessage = NULL;
	DWORD length = ::FormatMessageW(flags|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS, source, messageID, LocaleIDToLangID(langID), (LPWSTR)&formattedMessage, 0, NULL);
	if (length != 0 && formattedMessage != NULL)
	{
		std::wstring message(formattedMessage, length);
		::LocalFree(formattedMessage);
		return message;
	}
	return L"";
}
std::wstring KxVFSUtility::GetErrorMessage(DWORD code, WORD langID)
{
	std::wstring message = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, code, langID);
	return L"[" + std::to_wstring(code) + L"] " + message;
}

std::wstring KxVFSUtility::GetDriveFromPath(const WCHAR* path)
{
	if (path)
	{
		std::wstring_view pathView(path);
		size_t index = pathView.find(L':');
		if (index != std::wstring_view::npos && index >= 1)
		{
			WCHAR volumeRoot[4];
			volumeRoot[0] = path[index - 1];
			volumeRoot[1] = L':';
			volumeRoot[2] = L'\\';
			volumeRoot[3] = L'\0';
			return volumeRoot;
		}
	}
	return L"";
}
DWORD KxVFSUtility::GetFileAttributes(const WCHAR* path)
{
	return ::GetFileAttributesW(path);
}
bool KxVFSUtility::IsExist(const WCHAR* path)
{
	return ::GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}
bool KxVFSUtility::IsFileExist(const WCHAR* path)
{
	DWORD attributes = GetFileAttributesW(path);
	return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
bool KxVFSUtility::IsFolderExist(const WCHAR* path)
{
	DWORD attributes = GetFileAttributesW(path);
	return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool KxVFSUtility::CreateFolderTree(const WCHAR* pathW, bool noLast, SECURITY_ATTRIBUTES* securityAttributes, DWORD* errorCodeOut)
{
	if (!IsFolderExist(pathW))
	{
		std::wstring path(pathW);
		std::vector<std::wstring> folderArray;

		// Construct array
		const auto stdNOT_FOUND = std::wstring::npos;
		size_t folderStart = path.find(':');
		if (folderStart != stdNOT_FOUND)
		{
			if (path[folderStart+1] == '\\')
			{
				folderStart += 2;
			}
		}
		else
		{
			folderStart = 0;
		}

		size_t next = stdNOT_FOUND;
		do
		{
			next = path.find('\\', folderStart);
			folderArray.push_back(path.substr(folderStart, next - folderStart));
			folderStart = next + 1;
		}
		while (next != stdNOT_FOUND);

		// Create folder
		BOOL ret = FALSE;
		std::wstring fullPath = GetDriveFromPath(pathW) + L'\\';
		for (size_t i = 0; i < folderArray.size(); i++)
		{
			fullPath.append(folderArray[i]).append(L"\\");
			ret = ::CreateDirectoryW(fullPath.c_str(), securityAttributes);
			if (!ret)
			{
				SetIfNotNull(errorCodeOut, GetLastError());
			}

			if (noLast && i + 2 == folderArray.size())
			{
				break;
			}
		}
		return IsFolderExist(pathW);
	}

	SetIfNotNull(errorCodeOut, 0);
	return true;
}
bool KxVFSUtility::IsFolderEmpty(const WCHAR* path)
{
	return KxFileFinder::IsDirectoryEmpty(path);
}

int KxVFSUtility::DebugPrint(const WCHAR* fmt, ...)
{
	WCHAR buffer[INT16_MAX] = {0};

	va_list argptr;
	va_start(argptr, fmt);
	int count = vswprintf(buffer, ARRAYSIZE(buffer), fmt, argptr);
	va_end(argptr);

	::OutputDebugStringW(buffer);
	return count;
}
