#pragma once

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <memory>

#include "Misc/Setup.h"
#include "Misc/IncludeWindows.h"
#include "Utility/DynamicString/DynamicString.h"
#pragma warning(disable: 4251) // DLL interface

#if defined KxVFS_EXPORTS
	#define KxVFS_CEXPORT extern "C" __declspec(dllexport)
	#define KxVFS_API __declspec(dllexport)
#else
	#define KxVFS_CEXPORT extern "C" __declspec(dllimport)
	#define KxVFS_API __declspec(dllimport)
#endif
