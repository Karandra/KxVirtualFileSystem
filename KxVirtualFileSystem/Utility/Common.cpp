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
	uint16_t LocaleIDToLangID(uint16_t localeID)
	{
		return MAKELANGID(PRIMARYLANGID(localeID), SUBLANGID(localeID));
	}
	KxDynamicStringW FormatMessage(uint32_t flags, const void* source, uint32_t messageID, uint16_t langID)
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
	KxDynamicStringW GetErrorMessage(uint32_t code, uint16_t langID)
	{
		KxDynamicStringW message = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, code, langID);
		return FormatString(L"[%1] %2", code, message);
	}

	bool CreateDirectoryTree(KxDynamicStringRefW pathW, bool skipLastPart, SECURITY_ATTRIBUTES* securityAttributes, DWORD* errorCodeOut)
	{
		::SetLastError(ERROR_SUCCESS);

		if (!IsFolderExist(pathW))
		{
			KxDynamicStringW path = pathW;
			std::vector<KxDynamicStringW> folderArray;

			// Construct array
			const auto stdNOT_FOUND = KxDynamicStringRefW::npos;

			size_t folderStart = path.find(L':');
			if (folderStart != stdNOT_FOUND)
			{
				if (path[folderStart + 1] == L'\\')
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
				next = path.find(L'\\', folderStart);
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
				fullPath += L'\\';

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
	bool CreateDirectoryTreeEx(KxDynamicStringRefW baseDirectory, KxDynamicStringRefW path, SECURITY_ATTRIBUTES* securityAttributes)
	{
		::SetLastError(ERROR_SUCCESS);

		bool isSuccess = true;
		KxDynamicStringW fullPath = baseDirectory;
		String::SplitBySeparator(path, L'\\', [&fullPath, &isSuccess, securityAttributes](KxDynamicStringRefW directoryName)
		{
			fullPath += L'\\';
			fullPath += directoryName;
			
			if (!CreateDirectory(fullPath, securityAttributes))
			{
				const DWORD errorCode = ::GetLastError();
				isSuccess = errorCode == ERROR_ALREADY_EXISTS;
				return isSuccess;
			}
			return true;
		});
		return isSuccess;
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
	KxDynamicStringRefW NormalizeFilePath(KxDynamicStringRefW path)
	{
		// See if path starts with '\' and remove it. Don't touch '\\?\'.
		auto ExtractPrefix = [](KxDynamicStringRefW& path)
		{
			return path.substr(0, Utility::GetLongPathPrefix().size());
		};

		if (!path.empty() && ExtractPrefix(path) != Utility::GetLongPathPrefix())
		{
			size_t count = 0;
			for (const auto& c: path)
			{
				if (c == L'\\')
				{
					++count;
				}
				else
				{
					break;
				}
			}
			path.remove_prefix(count);
		}

		// Remove any trailing '\\'
		size_t count = 0;
		for (auto it = path.rbegin(); it != path.rend(); ++it)
		{
			if (*it == L'\\')
			{
				++count;
			}
			else
			{
				break;
			}
		}
		path.remove_suffix(count);
		return path;
	}
	KxDynamicStringW ExpandEnvironmentStrings(KxDynamicStringRefW variables)
	{
		DWORD requiredLength = ::ExpandEnvironmentStringsW(variables.data(), nullptr, 0);
		if (requiredLength != 0)
		{
			KxDynamicStringW expandedString;
			expandedString.resize(requiredLength - 1);
			::ExpandEnvironmentStringsW(variables.data(), expandedString.data(), requiredLength);

			return expandedString;
		}
		return {};
	}
	
	size_t WriteString(KxDynamicStringRefW source, wchar_t* destination, const size_t maxDstLength)
	{
		const size_t dstBytesLength = maxDstLength * sizeof(wchar_t);
		const size_t srcBytesLength = std::min(dstBytesLength, source.length() * sizeof(wchar_t));

		memcpy_s(destination, dstBytesLength, source.data(), srcBytesLength);
		return srcBytesLength / sizeof(wchar_t);
	}
	KxDynamicStringRefW ExceptionCodeToString(uint32_t code)
	{
		switch (code)
		{
			case ToInt(Dokany2::ExceptionCode::NotInitialized):
			{
				return L"<Dokany> NotInitialized";
			}
			case ToInt(Dokany2::ExceptionCode::InitializationFailed):
			{
				return L"<Dokany> InitializationFailed";
			}
			case ToInt(Dokany2::ExceptionCode::ShutdownFailed):
			{
				return L"<Dokany> ShutdownFailed";
			}

			case EXCEPTION_ACCESS_VIOLATION:
			{
				return L"<System> EXCEPTION_ACCESS_VIOLATION";
			}
			case EXCEPTION_DATATYPE_MISALIGNMENT:
			{
				return L"<System> EXCEPTION_DATATYPE_MISALIGNMENT";
			}
			case EXCEPTION_BREAKPOINT:
			{
				return L"<System> EXCEPTION_BREAKPOINT";
			}
			case EXCEPTION_SINGLE_STEP:
			{
				return L"<System> EXCEPTION_SINGLE_STEP";
			}
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			{
				return L"<System> EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
			}
			case EXCEPTION_FLT_DENORMAL_OPERAND:
			{
				return L"<System> EXCEPTION_FLT_DENORMAL_OPERAND";
			}
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			{
				return L"<System> EXCEPTION_FLT_DIVIDE_BY_ZERO";
			}
			case EXCEPTION_FLT_INEXACT_RESULT:
			{
				return L"<System> EXCEPTION_FLT_INEXACT_RESULT";
			}
			case EXCEPTION_FLT_INVALID_OPERATION:
			{
				return L"<System> EXCEPTION_FLT_INVALID_OPERATION";
			}
			case EXCEPTION_FLT_OVERFLOW:
			{
				return L"<System> EXCEPTION_FLT_OVERFLOW";
			}
			case EXCEPTION_FLT_STACK_CHECK:
			{
				return L"<System> EXCEPTION_FLT_STACK_CHECK";
			}
			case EXCEPTION_FLT_UNDERFLOW:
			{
				return L"<System> EXCEPTION_FLT_UNDERFLOW";
			}
			case EXCEPTION_INT_DIVIDE_BY_ZERO:
			{
				return L"<System> EXCEPTION_INT_DIVIDE_BY_ZERO";
			}
			case EXCEPTION_INT_OVERFLOW:
			{
				return L"<System> EXCEPTION_INT_OVERFLOW";
			}
			case EXCEPTION_PRIV_INSTRUCTION:
			{
				return L"<System> EXCEPTION_PRIV_INSTRUCTION";
			}
			case EXCEPTION_IN_PAGE_ERROR:
			{
				return L"<System> EXCEPTION_IN_PAGE_ERROR";
			}
			case EXCEPTION_ILLEGAL_INSTRUCTION:
			{
				return L"<System> EXCEPTION_ILLEGAL_INSTRUCTION";
			}
			case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			{
				return L"<System> EXCEPTION_NONCONTINUABLE_EXCEPTION";
			}
			case EXCEPTION_STACK_OVERFLOW:
			{
				return L"<System> EXCEPTION_STACK_OVERFLOW";
			}
			case EXCEPTION_INVALID_DISPOSITION:
			{
				return L"<System> EXCEPTION_INVALID_DISPOSITION";
			}
			case EXCEPTION_GUARD_PAGE:
			{
				return L"<System> EXCEPTION_GUARD_PAGE";
			}
			case EXCEPTION_INVALID_HANDLE:
			{
				return L"<System> EXCEPTION_INVALID_HANDLE";
			}
			case EXCEPTION_POSSIBLE_DEADLOCK:
			{
				return L"<System> EXCEPTION_POSSIBLE_DEADLOCK";
			}
		};
		return L"<Unknown>";
	}
}
