#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility/KxDynamicString/KxBasicDynamicString.h"

namespace KxVFS
{
	using KxDynamicStringA = KxBasicDynamicString<char, MAX_PATH>;
	using KxDynamicStringRefA = typename KxDynamicStringA::TStringView;

	using KxDynamicStringW = KxBasicDynamicString<wchar_t, MAX_PATH>;
	using KxDynamicStringRefW = typename KxDynamicStringW::TStringView;
}

namespace KxVFS
{
	extern const KxDynamicStringA KxNullDynamicStringA;
	extern const KxDynamicStringRefA KxNullDynamicStringRefA;

	extern const KxDynamicStringW KxNullDynamicStringW;
	extern const KxDynamicStringRefW KxNullDynamicStringRefW;
}
