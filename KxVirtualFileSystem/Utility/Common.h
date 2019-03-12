/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/KxDynamicString/KxDynamicString.h"

namespace KxVFS::Utility
{
	using StringSearcherHash = std::unordered_set<size_t>;

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

	bool CreateFolderTree(KxDynamicStringRefW pathW, bool skipLastPart = false, SECURITY_ATTRIBUTES* securityAttributes = nullptr, DWORD* errorCodeOut = nullptr);
	bool IsFolderEmpty(KxDynamicStringRefW path);
	KxDynamicStringW GetDriveFromPath(KxDynamicStringRefW path);
	DWORD GetFileAttributes(KxDynamicStringRefW path);
	bool IsExist(KxDynamicStringRefW path);
	bool IsFileExist(KxDynamicStringRefW path);
	bool IsFolderExist(KxDynamicStringRefW path);

	inline size_t HashString(KxDynamicStringRefW string)
	{
		return std::hash<KxDynamicStringRefW>()(string);
	}

	template<class TInt64, class TInt32> void Int64ToHighLow(const TInt64 value, TInt32& high, TInt32& low)
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");
		static_assert(sizeof(TInt32) == sizeof(uint32_t), "High and low values must be 32-bit integers");

		LARGE_INTEGER largeInt;
		largeInt.QuadPart = value;

		high = largeInt.HighPart;
		low = largeInt.LowPart;
	}
	template<class TInt64> void Int64ToOverlappedOffset(const TInt64 offset, OVERLAPPED& overlapped)
	{
		Utility::Int64ToHighLow(offset, overlapped.OffsetHigh, overlapped.Offset);
	}

	template<class TInt64, class TInt32> void HighLowToInt64(TInt64& value, const TInt32 high, const TInt32 low)
	{
		static_assert(sizeof(TInt64) == sizeof(uint64_t), "Value must be 64-bit integer");
		static_assert(sizeof(TInt32) == sizeof(uint32_t), "High and low values must be 32-bit integers");

		LARGE_INTEGER largeInt;
		largeInt.HighPart = high;
		largeInt.LowPart = low;
		value = largeInt.QuadPart;
	}
	template<class TInt64, class TInt32> TInt64 HighLowToInt64(const TInt32 high, const TInt32 low)
	{
		TInt64 value = 0;
		HighLowToInt64(value, high, low);
		return value;
	}

	template<class T> void MoveValue(T& left, T& right, T resetTo = {})
	{
		left = right;
		right = resetTo;
	}
}

namespace KxVFS::Utility
{
	inline char CharToLower(char c)
	{
		#pragma warning(suppress: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
		#pragma warning(suppress: 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
		return reinterpret_cast<char>(::CharLowerA(reinterpret_cast<LPSTR>(c)));
	}
	inline char CharToUpper(char c)
	{
		#pragma warning(suppress: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
		#pragma warning(suppress: 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
		return reinterpret_cast<char>(::CharLowerA(reinterpret_cast<LPSTR>(c)));
	}
	inline wchar_t CharToLower(wchar_t c)
	{
		#pragma warning(suppress: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
		#pragma warning(suppress: 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
		return reinterpret_cast<wchar_t>(::CharLowerW(reinterpret_cast<LPWSTR>(c)));
	}
	inline wchar_t CharToUpper(wchar_t c)
	{
		#pragma warning(suppress: 4312) // 'operation' : conversion from 'type1' to 'type2' of greater size
		#pragma warning(suppress: 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
		return reinterpret_cast<wchar_t>(::CharLowerW(reinterpret_cast<LPWSTR>(c)));
	}
}

namespace KxVFS::Utility
{
	template<class... Args> size_t DebugPrint(const wchar_t* fmt, Args&&... arg)
	{
		KxDynamicStringW output = KxDynamicStringW::Format(fmt, std::forward<Args>(arg)...);
		output += L"\r\n";

		::OutputDebugStringW(output.data());

		DWORD writtenCount = 0;
		::WriteConsoleW(::GetStdHandle(STD_OUTPUT_HANDLE), output.data(), static_cast<DWORD>(output.size()), &writtenCount, nullptr);
		return output.length();
	}
};

#ifdef _DEBUG
	#define KxVFSDebugPrint KxVFS::Utility::DebugPrint
#else
	#define KxVFSDebugPrint
#endif
