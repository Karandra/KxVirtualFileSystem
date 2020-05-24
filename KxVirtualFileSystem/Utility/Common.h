#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "DynamicString/DynamicString.h"
#include "Formatter/Formatter.h"
#include "Win32Constants.h"

namespace KxVFS::Utility::Private
{
	constexpr wchar_t LongPathPrefix[] = L"\\\\?\\";
}

namespace KxVFS::Utility
{
	uint16_t LocaleIDToLangID(uint16_t localeID) noexcept;
	DynamicStringW FormatMessage(uint32_t flags, const void* source, uint32_t messageID, uint16_t langID = 0);
	DynamicStringW GetErrorMessage(uint32_t code = ::GetLastError(), uint16_t langID = 0);

	template<class T1, class T2>
	static bool SetIfNotNull(T1* pointer, T2&& value) noexcept
	{
		if (pointer)
		{
			*pointer = value;
			return true;
		}
		return false;
	}

	template<class TFlag, class TFlagValue>
	static TFlag ModFlag(TFlag flag, TFlagValue value, bool set) noexcept
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
	
	template<class TFlag, class TFlagValue>
	static void ModFlagRef(TFlag& flag, TFlagValue value, bool set) noexcept
	{
		flag = ModFlag(flag, value, set);
	}
	
	template<class TFlag, class TFlagValue>
	static bool HasFlag(TFlag flag, TFlagValue value) noexcept
	{
		return flag & value;
	}

	inline bool CreateDirectory(DynamicStringRefW path, SECURITY_ATTRIBUTES* securityAttributes = nullptr) noexcept
	{
		return ::CreateDirectoryW(path.data(), securityAttributes);
	}
	bool CreateDirectoryTree(DynamicStringRefW pathW, bool skipLastPart = false, SECURITY_ATTRIBUTES* securityAttributes = nullptr, DWORD* errorCodeOut = nullptr);
	bool CreateDirectoryTreeEx(DynamicStringRefW baseDirectory, DynamicStringRefW path, SECURITY_ATTRIBUTES* securityAttributes = nullptr);

	inline FileAttributes GetFileAttributes(DynamicStringRefW path) noexcept
	{
		return FromInt<FileAttributes>(::GetFileAttributesW(path.data()));
	}
	inline bool IsAnyExist(DynamicStringRefW path) noexcept
	{
		return GetFileAttributes(path) != FileAttributes::Invalid;
	}
	inline bool IsFileExist(DynamicStringRefW path) noexcept
	{
		const FileAttributes attributes = GetFileAttributes(path);
		return attributes != FileAttributes::Invalid && !(attributes & FileAttributes::Directory);
	}
	inline bool IsFolderExist(DynamicStringRefW path) noexcept
	{
		const FileAttributes attributes = GetFileAttributes(path);
		return attributes != FileAttributes::Invalid && attributes & FileAttributes::Directory;
	}

	constexpr inline DynamicStringRefW GetLongPathPrefix() noexcept
	{
		return DynamicStringRefW(Private::LongPathPrefix, std::size(Private::LongPathPrefix) - 1);
	}
	inline DynamicStringW GetVolumeDevicePath(wchar_t volumeLetter) noexcept
	{
		DynamicStringW path = L"\\\\.\\";
		path += tolower(volumeLetter);
		path += L':';

		return path;
	}
	DynamicStringW GetDriveFromPath(DynamicStringRefW path) noexcept;
	DynamicStringRefW NormalizeFilePath(DynamicStringRefW path);

	DynamicStringW ExpandEnvironmentStrings(DynamicStringRefW variables);
	DynamicStringRefW ExceptionCodeToString(uint32_t code);

	// Writes a string 'source' into specified buffer but no more than 'maxDstLength' CHARS. Returns number of BYTES written.
	size_t WriteString(DynamicStringRefW source, wchar_t* destination, const size_t maxDstLength) noexcept;

	inline size_t HashString(DynamicStringRefW string) noexcept
	{
		return std::hash<DynamicStringRefW>()(string);
	}

	template<class T>
	void MoveValue(T& left, T& right, T resetTo = {}) noexcept(std::is_nothrow_move_assignable_v<T>)
	{
		left = std::move(right);
		right = std::move(resetTo);
	}
	
	template<class T>
	void CopyMemory(T* destination, const T* source, size_t length) noexcept
	{
		std::memcpy(destination, source, length * sizeof(T));
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
	template<class TInt64>
	void LowHighToInt64(TInt64& value, uint32_t low, int32_t high) noexcept
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");

		LARGE_INTEGER largeInt = {};
		largeInt.LowPart = low;
		largeInt.HighPart = high;
		value = largeInt.QuadPart;
	}
	
	inline int64_t LowHighToInt64(uint32_t low, int32_t high) noexcept
	{
		int64_t value = 0;
		LowHighToInt64(value, low, high);
		return value;
	}
	
	template<class L, class H>
	void Int64ToLowHigh(int64_t value, L& low, H& high) noexcept
	{
		static_assert(sizeof(L) == sizeof(int32_t) && sizeof(H) == sizeof(int32_t), "Components must be 32-bit integers");

		LARGE_INTEGER largeInt = {0};
		largeInt.QuadPart = value;

		low = largeInt.LowPart;
		high = largeInt.HighPart;
	}

	inline int64_t OverlappedOffsetToInt64(const OVERLAPPED& overlapped) noexcept
	{
		return LowHighToInt64(overlapped.Offset, overlapped.OffsetHigh);
	}
	
	template<class TInt64>
	void OverlappedOffsetToInt64(const OVERLAPPED& overlapped, TInt64& offset) noexcept
	{
		LowHighToInt64(offset, overlapped.Offset, overlapped.OffsetHigh);
	}
	
	inline void Int64ToOverlappedOffset(int64_t offset, OVERLAPPED& overlapped) noexcept
	{
		Int64ToLowHigh(offset, overlapped.Offset, overlapped.OffsetHigh);
	}
}

namespace KxVFS::Utility
{
	#pragma warning(push)
	#pragma warning(disable: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
	#pragma warning(disable: 4302) // 'conversion' : truncation from 'type1' to 'type2'

	inline char CharToLower(char c) noexcept
	{
		return reinterpret_cast<char>(::CharLowerA(reinterpret_cast<LPSTR>(c)));
	}
	inline char CharToUpper(char c) noexcept
	{
		return reinterpret_cast<char>(::CharUpperA(reinterpret_cast<LPSTR>(c)));
	}

	inline wchar_t CharToLower(wchar_t c) noexcept
	{
		return reinterpret_cast<wchar_t>(::CharLowerW(reinterpret_cast<LPWSTR>(c)));
	}
	inline wchar_t CharToUpper(wchar_t c) noexcept
	{
		return reinterpret_cast<wchar_t>(::CharUpperW(reinterpret_cast<LPWSTR>(c)));
	}

	#pragma warning(pop)
}

namespace KxVFS::Utility
{
	#pragma warning(push)
	#pragma warning(disable: 4267) // 'argument': conversion from 'size_t' to 'DWORD', possible loss of data

	inline void StringToLower(DynamicStringA& value) noexcept
	{
		::CharLowerBuffA(value.data(), value.length());
	}
	inline DynamicStringA StringToLower(const DynamicStringA& value)
	{
		DynamicStringA copy = value;
		StringToLower(copy);
		return copy;
	}

	inline void StringToLower(DynamicStringW& value) noexcept
	{
		::CharLowerBuffW(value.data(), value.length());
	}
	inline DynamicStringW StringToLower(const DynamicStringW& value)
	{
		DynamicStringW copy = value;
		StringToLower(copy);
		return copy;
	}

	inline void StringToUpper(DynamicStringA& value) noexcept
	{
		::CharUpperBuffA(value.data(), value.length());
	}
	inline DynamicStringA StringToUpper(const DynamicStringA& value)
	{
		DynamicStringA copy = value;
		StringToUpper(copy);
		return copy;
	}
	
	inline void StringToUpper(DynamicStringW& value) noexcept
	{
		::CharUpperBuffW(value.data(), value.length());
	}
	inline DynamicStringW StringToUpper(const DynamicStringW& value)
	{
		DynamicStringW copy = value;
		StringToUpper(copy);
		return copy;
	}

	#pragma warning(pop)
}


namespace KxVFS::Utility
{
	template<class... Args>
	static DynamicStringW FormatString(const wchar_t* format, Args&&... arg)
	{
		if constexpr((sizeof...(Args)) != 0)
		{
			Formatter formatter(format);
			std::initializer_list<int>{((void)formatter(std::forward<Args>(arg)), 0)...};
			return formatter;
		}
		return format;
	}
}
