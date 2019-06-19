/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxDynamicString/KxDynamicString.h"
#include "Win32Constants.h"

namespace KxVFS::Utility::Internal
{
	const wchar_t LongPathPrefix[] = L"\\\\?\\";
}

namespace KxVFS::Utility
{
	WORD LocaleIDToLangID(WORD localeID);
	KxDynamicStringW FormatMessage(DWORD flags, const void* source, DWORD messageID, WORD langID = 0);
	KxDynamicStringW GetErrorMessage(DWORD code = GetLastError(), WORD langID = 0);

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
}

namespace KxVFS::Utility
{
	template<class TInt64> void HighLowToInt64(TInt64& value, int32_t high, uint32_t low)
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");

		LARGE_INTEGER largeInt = {};
		largeInt.HighPart = high;
		largeInt.LowPart = low;
		value = largeInt.QuadPart;
	}
	inline int64_t HighLowToInt64(int32_t high, uint32_t low)
	{
		int64_t value = 0;
		HighLowToInt64(value, high, low);
		return value;
	}

	template<class TInt64, class TInt32H = int32_t, class TInt32L = uint32_t>
	void Int64ToHighLow(TInt64 value, TInt32H& high, TInt32L& low)
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");

		LARGE_INTEGER largeInt = {0};
		largeInt.QuadPart = value;

		high = largeInt.HighPart;
		low = largeInt.LowPart;
	}

	template<class TInt64 = int64_t> TInt64 OverlappedOffsetToInt64(const OVERLAPPED& overlapped)
	{
		return HighLowToInt64<TInt64>(overlapped.OffsetHigh, overlapped.Offset);
	}
	template<class TInt64> void OverlappedOffsetToInt64(TInt64& offset, const OVERLAPPED& overlapped)
	{
		HighLowToInt64(offset, overlapped.OffsetHigh, overlapped.Offset);
	}
	template<class TInt64> void Int64ToOverlappedOffset(const TInt64 offset, OVERLAPPED& overlapped)
	{
		Int64ToHighLow(offset, overlapped.OffsetHigh, overlapped.Offset);
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
	size_t Print(KxDynamicStringRefA text);
	size_t Print(KxDynamicStringRefW text);

	template<class... Args> size_t Print(const char* format, Args&&... arg)
	{
		KxDynamicStringA text = KxDynamicStringA::Format("[Thread:%u] ", ::GetCurrentThreadId());
		text += KxDynamicStringA::Format(format, std::forward<Args>(arg)...);
		text += "\r\n";

		return Print(text);
	}
	template<class... Args> size_t Print(const wchar_t* format, Args&&... arg)
	{
		KxDynamicStringW text = KxDynamicStringW::Format(L"[Thread:%u] ", ::GetCurrentThreadId());
		text += KxDynamicStringW::Format(format, std::forward<Args>(arg)...);
		text += L"\r\n";

		return Print(text);
	}
};

#if KxVFS_DEBUG_ENABLE_LOG
	#define KxVFS_DebugPrint KxVFS::Utility::Print
#else
	#define KxVFS_DebugPrint
#endif
