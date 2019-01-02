/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

#define _CRT_SECURE_NO_WARNINGS 1
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <locale>
#include <algorithm>
#include <functional>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>

#include "IncludeWindows.h"
#pragma warning(disable: 4251) // DLL interface

#define KxVFS_USE_ASYNC_IO 1

#if defined KxVFS_EXPORTS
	#define KxVFS_CEXPORT extern "C" __declspec(dllexport)
	#define KxVFS_API __declspec(dllexport)
#else
	#define KxVFS_CEXPORT extern "C" __declspec(dllimport)
	#define KxVFS_API __declspec(dllimport)
#endif
