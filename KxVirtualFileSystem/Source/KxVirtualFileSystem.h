#define _CRT_SECURE_NO_WARNINGS 1
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <string>
#include <locale>
#include <algorithm>
#include <functional>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "KxVFSIncludeWindows.h"
#pragma warning(disable: 4251) // DLL interface

#define KxVFS_USE_ASYNC_IO	1
#define KxVFS_CAPI

#if defined KxVFS_EXPORTS
	#define KxVFS_CEXPORT extern "C" __declspec(dllexport)
	#define KxVFS_API __declspec(dllexport)
#else
	#define KxVFS_CEXPORT extern "C" __declspec(dllimport)
	#define KxVFS_API __declspec(dllimport)
#endif

#define KxVFS_MAKELONGLONG(hi, lo)		((LONGLONG(DWORD(hi) & 0xffffffff) << 32 ) | LONGLONG(DWORD(lo) & 0xffffffff))
#define KxVFS_UNUSED(v)					(void)v
