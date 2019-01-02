/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxDynamicString/KxDynamicString.h"

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
	
	template<class T, class V> static bool SetIfNotNull(T* p, const V& v)
	{
		if (p)
		{
			*p = v;
			return true;
		}
		return false;
	}

	bool CreateFolderTree(KxDynamicStringRefW pathW, bool noLast = false, SECURITY_ATTRIBUTES* securityAttributes = nullptr, DWORD* errorCodeOut = nullptr);
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
}

namespace KxVFS::Utility
{
	template<class... Args> size_t DebugPrint(const wchar_t* fmt, Args&&... arg)
	{
		KxDynamicStringW output = KxDynamicStringW::Format(fmt, std::forward<Args>(arg)...);

		output += L"\r\n";
		::OutputDebugStringW(output.data());
		::_putws(output.data());
		return output.length();
	}
};

#ifdef _DEBUG
	#define KxVFSDebugPrint KxVFS::Utility::DebugPrint
#else
	#define KxVFSDebugPrint
#endif
