/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxDynamicString/KxDynamicString.h"
#include "KxFormat/KxFormat.h"
#include "Win32Constants.h"

namespace KxVFS::Utility::Internal
{
	const wchar_t LongPathPrefix[] = L"\\\\?\\";
}

namespace KxVFS::Utility
{
	uint16_t LocaleIDToLangID(uint16_t localeID);
	KxDynamicStringW FormatMessage(uint32_t flags, const void* source, uint32_t messageID, uint16_t langID = 0);
	KxDynamicStringW GetErrorMessage(uint32_t code = ::GetLastError(), uint16_t langID = 0);

	template<class T1, class T2> static bool SetIfNotNull(T1* pointer, T2&& value)
	{
		if (pointer)
		{
			*pointer = value;
			return true;
		}
		return false;
	}

	template<class TFlag, class TFlagValue> static TFlag ModFlag(TFlag flag, TFlagValue value, bool set)
	{
		if (set)
		{
			flag = static_cast<TFlag>(flag|static_cast<TFlag>(value));
		}
		else
		{
			flag = static_cast<TFlag>(flag & ~static_cast<TFlag>(value));
		}
		return flag;
	}
	template<class TFlag, class TFlagValue> static void ModFlagRef(TFlag& flag, TFlagValue value, bool set)
	{
		flag = ModFlag(flag, value, set);
	}
	template<class TFlag, class TFlagValue> static bool HasFlag(TFlag flag, TFlagValue value)
	{
		return flag & value;
	}

	inline bool CreateDirectory(KxDynamicStringRefW path, SECURITY_ATTRIBUTES* securityAttributes = nullptr)
	{
		return ::CreateDirectoryW(path.data(), securityAttributes);
	}
	bool CreateDirectoryTree(KxDynamicStringRefW pathW, bool skipLastPart = false, SECURITY_ATTRIBUTES* securityAttributes = nullptr, DWORD* errorCodeOut = nullptr);
	bool CreateDirectoryTreeEx(KxDynamicStringRefW baseDirectory, KxDynamicStringRefW path, SECURITY_ATTRIBUTES* securityAttributes = nullptr);

	inline FileAttributes GetFileAttributes(KxDynamicStringRefW path)
	{
		return FromInt<FileAttributes>(::GetFileAttributesW(path.data()));
	}
	inline bool IsAnyExist(KxDynamicStringRefW path)
	{
		return GetFileAttributes(path) != FileAttributes::Invalid;
	}
	inline bool IsFileExist(KxDynamicStringRefW path)
	{
		const FileAttributes attributes = GetFileAttributes(path);
		return attributes != FileAttributes::Invalid && !(attributes & FileAttributes::Directory);
	}
	inline bool IsFolderExist(KxDynamicStringRefW path)
	{
		const FileAttributes attributes = GetFileAttributes(path);
		return attributes != FileAttributes::Invalid && attributes & FileAttributes::Directory;
	}

	constexpr inline KxDynamicStringRefW GetLongPathPrefix()
	{
		return KxDynamicStringRefW(Internal::LongPathPrefix, std::size(Internal::LongPathPrefix) - 1);
	}
	inline KxDynamicStringW GetVolumeDevicePath(wchar_t volumeLetter)
	{
		KxDynamicStringW path = L"\\\\.\\";
		path += tolower(volumeLetter);
		path += L':';

		return path;
	}
	KxDynamicStringW GetDriveFromPath(KxDynamicStringRefW path);
	KxDynamicStringRefW NormalizeFilePath(KxDynamicStringRefW path);

	KxDynamicStringW ExpandEnvironmentStrings(KxDynamicStringRefW variables);
	KxDynamicStringRefW ExceptionCodeToString(uint32_t code);

	// Writes a string 'source' into specified buffer but no more than 'maxDstLength' CHARS. Returns number of BYTES written.
	size_t WriteString(KxDynamicStringRefW source, wchar_t* destination, const size_t maxDstLength);

	inline size_t HashString(KxDynamicStringRefW string)
	{
		return std::hash<KxDynamicStringRefW>()(string);
	}

	template<class T> void MoveValue(T& left, T& right, T resetTo = {})
	{
		left = right;
		right = resetTo;
	}
	template<class T> void CopyMemory(T* destination, const T* source, size_t length)
	{
		memcpy(destination, source, length * sizeof(T));
	}

	template<class TFuncTry, class TFuncExcept>
	bool SEHTryExcept(TFuncTry&& funcTry, TFuncExcept&& funcExcept)
	{
		__try
		{
			std::invoke(funcTry);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			std::invoke(funcExcept, GetExceptionCode());
			return false;
		}
	}
}

namespace KxVFS::Utility
{
	template<class TInt64> void LowHighToInt64(TInt64& value, uint32_t low, int32_t high)
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");

		LARGE_INTEGER largeInt = {};
		largeInt.LowPart = low;
		largeInt.HighPart = high;
		value = largeInt.QuadPart;
	}
	inline int64_t LowHighToInt64(uint32_t low, int32_t high)
	{
		int64_t value = 0;
		LowHighToInt64(value, low, high);
		return value;
	}
	template<class L, class H> void Int64ToLowHigh(int64_t value, L& low, H& high)
	{
		static_assert(sizeof(L) == sizeof(int32_t) && sizeof(H) == sizeof(int32_t), "Components must be 32-bit integers");

		LARGE_INTEGER largeInt = {0};
		largeInt.QuadPart = value;

		low = largeInt.LowPart;
		high = largeInt.HighPart;
	}

	inline int64_t OverlappedOffsetToInt64(const OVERLAPPED& overlapped)
	{
		return LowHighToInt64(overlapped.Offset, overlapped.OffsetHigh);
	}
	template<class TInt64> void OverlappedOffsetToInt64(const OVERLAPPED& overlapped, TInt64& offset)
	{
		LowHighToInt64(offset, overlapped.Offset, overlapped.OffsetHigh);
	}
	inline void Int64ToOverlappedOffset(int64_t offset, OVERLAPPED& overlapped)
	{
		Int64ToLowHigh(offset, overlapped.Offset, overlapped.OffsetHigh);
	}
}

namespace KxVFS::Utility
{
	#pragma warning(push)
	#pragma warning(disable: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
	#pragma warning(disable: 4302) // 'conversion' : truncation from 'type1' to 'type2'

	inline char CharToLower(char c)
	{
		return reinterpret_cast<char>(::CharLowerA(reinterpret_cast<LPSTR>(c)));
	}
	inline char CharToUpper(char c)
	{
		return reinterpret_cast<char>(::CharUpperA(reinterpret_cast<LPSTR>(c)));
	}

	inline wchar_t CharToLower(wchar_t c)
	{
		return reinterpret_cast<wchar_t>(::CharLowerW(reinterpret_cast<LPWSTR>(c)));
	}
	inline wchar_t CharToUpper(wchar_t c)
	{
		return reinterpret_cast<wchar_t>(::CharUpperW(reinterpret_cast<LPWSTR>(c)));
	}

	#pragma warning(pop)
}

namespace KxVFS::Utility
{
	#pragma warning(push)
	#pragma warning(disable: 4267) // 'argument': conversion from 'size_t' to 'DWORD', possible loss of data

	inline void StringToLower(KxDynamicStringA& value)
	{
		::CharLowerBuffA(value.data(), value.length());
	}
	inline KxDynamicStringA StringToLower(const KxDynamicStringA& value)
	{
		KxDynamicStringA copy = value;
		StringToLower(copy);
		return copy;
	}

	inline void StringToLower(KxDynamicStringW& value)
	{
		::CharLowerBuffW(value.data(), value.length());
	}
	inline KxDynamicStringW StringToLower(const KxDynamicStringW& value)
	{
		KxDynamicStringW copy = value;
		StringToLower(copy);
		return copy;
	}

	inline void StringToUpper(KxDynamicStringA& value)
	{
		::CharUpperBuffA(value.data(), value.length());
	}
	inline KxDynamicStringA StringToUpper(const KxDynamicStringA& value)
	{
		KxDynamicStringA copy = value;
		StringToUpper(copy);
		return copy;
	}
	
	inline void StringToUpper(KxDynamicStringW& value)
	{
		::CharUpperBuffW(value.data(), value.length());
	}
	inline KxDynamicStringW StringToUpper(const KxDynamicStringW& value)
	{
		KxDynamicStringW copy = value;
		StringToUpper(copy);
		return copy;
	}

	#pragma warning(pop)
}


namespace KxVFS::Utility
{
	template<class... Args>
	static KxDynamicStringW FormatString(const wchar_t* format, Args&&... arg)
	{
		if constexpr((sizeof...(Args)) != 0)
		{
			KxFormat formatter(format);
			std::initializer_list<int>{((void)formatter(std::forward<Args>(arg)), 0)...};
			return formatter;
		}
		return format;
	}
}
