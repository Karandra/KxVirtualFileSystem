#pragma once
#include "KxVFS/Common.hpp"
#include "BasicDynamicString.h"

namespace KxVFS
{
	using DynamicStringA = BasicDynamicString<char, MAX_PATH>;
	using DynamicStringRefA = typename DynamicStringA::TStringView;

	using DynamicStringW = BasicDynamicString<wchar_t, MAX_PATH>;
	using DynamicStringRefW = typename DynamicStringW::TStringView;
}

namespace KxVFS
{
	extern const DynamicStringA NullDynamicStringA;
	extern const DynamicStringW NullDynamicStringW;
}
