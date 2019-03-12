/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "Common.h"

namespace KxVFS::Utility
{
	WORD LocaleIDToLangID(WORD localeID)
	{
		return MAKELANGID(PRIMARYLANGID(localeID), SUBLANGID(localeID));
	}
	KxDynamicStringW FormatMessage(DWORD flags, const void* source, DWORD messageID, WORD langID)
	{
		LPWSTR formattedMessage = nullptr;
		DWORD length = ::FormatMessageW(flags|FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_IGNORE_INSERTS, source, messageID, LocaleIDToLangID(langID), (LPWSTR)&formattedMessage, 0, nullptr);
		if (length != 0 && formattedMessage != nullptr)
		{
			KxDynamicStringW message(formattedMessage, length);
			::LocalFree(formattedMessage);
			return message;
		}
		return L"";
	}
	KxDynamicStringW GetErrorMessage(DWORD code, WORD langID)
	{
		KxDynamicStringW message = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, code, langID);
		return KxDynamicStringW::Format(L"[%u] %s", code, message.data());
	}

	KxDynamicStringW GetDriveFromPath(KxDynamicStringRefW path)
	{
		if (!path.empty())
		{
			size_t index = path.find(L':');
			if (index != KxDynamicStringRefW::npos && index >= 1)
			{
				WCHAR volumeRoot[4] = {0};
				volumeRoot[0] = path[index - 1];
				volumeRoot[1] = L':';
				volumeRoot[2] = L'\\';
				volumeRoot[3] = L'\0';
				return volumeRoot;
			}
		}
		return L"";
	}
	DWORD GetFileAttributes(KxDynamicStringRefW path)
	{
		return ::GetFileAttributesW(path.data());
	}
	bool IsExist(KxDynamicStringRefW path)
	{
		return ::GetFileAttributesW(path.data()) != INVALID_FILE_ATTRIBUTES;
	}
	bool IsFileExist(KxDynamicStringRefW path)
	{
		DWORD attributes = GetFileAttributesW(path.data());
		return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
	}
	bool IsFolderExist(KxDynamicStringRefW path)
	{
		DWORD attributes = GetFileAttributesW(path.data());
		return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool CreateFolderTree(KxDynamicStringRefW pathW, bool skipLastPart, SECURITY_ATTRIBUTES* securityAttributes, DWORD* errorCodeOut)
	{
		if (!IsFolderExist(pathW))
		{
			KxDynamicStringW path = pathW;
			std::vector<KxDynamicStringW> folderArray;

			// Construct array
			const auto stdNOT_FOUND = KxDynamicStringRefW::npos;

			size_t folderStart = path.find(':');
			if (folderStart != stdNOT_FOUND)
			{
				if (path[folderStart + 1] == '\\')
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
			KxDynamicStringW fullPath = GetDriveFromPath(pathW);
			for (size_t i = 0; i < folderArray.size(); i++)
			{
				fullPath += folderArray[i];
				fullPath += L"\\";

				ret = ::CreateDirectoryW(fullPath.c_str(), securityAttributes);
				if (!ret)
				{
					SetIfNotNull(errorCodeOut, GetLastError());
				}

				if (skipLastPart && i + 2 == folderArray.size())
				{
					break;
				}
			}
			return IsFolderExist(pathW);
		}

		SetIfNotNull(errorCodeOut, 0);
		return true;
	}
	bool IsFolderEmpty(KxDynamicStringRefW path)
	{
		return KxFileFinder::IsDirectoryEmpty(path);
	}
}
