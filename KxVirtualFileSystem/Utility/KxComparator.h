/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxFramework. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "Utility.h"

namespace KxVFS::Utility::Comparator::Internal
{
	enum class CompareResult: int
	{
		LessThan = CSTR_LESS_THAN,
		Equal = CSTR_EQUAL,
		GreaterThan = CSTR_GREATER_THAN,
	};

	inline CompareResult CompareStrings(KxDynamicStringRefW v1, KxDynamicStringRefW v2, bool ignoreCase)
	{
		return static_cast<CompareResult>(::CompareStringOrdinal(v1.data(), static_cast<int>(v1.length()), v2.data(), static_cast<int>(v2.length()), ignoreCase));
	}
	inline CompareResult CompareStrings(KxDynamicStringRefW v1, KxDynamicStringRefW v2)
	{
		return CompareStrings(v1, v2, false);
	}
	inline CompareResult CompareStringsNoCase(KxDynamicStringRefW v1, KxDynamicStringRefW v2)
	{
		return CompareStrings(v1, v2, true);
	}
}

namespace KxVFS::Utility::Comparator
{
	// ==
	template<class T> bool IsEqual(const T& v1, const T& v2)
	{
		return v1 == v2;
	}
	inline bool IsEqual(KxDynamicStringRefW v1, KxDynamicStringRefW v2)
	{
		return Internal::CompareStrings(v1, v2) == Internal::CompareResult::Equal;
	}
	inline bool IsEqualNoCase(KxDynamicStringRefW v1, KxDynamicStringRefW v2)
	{
		return Internal::CompareStringsNoCase(v1, v2) == Internal::CompareResult::Equal;
	}
}

namespace KxVFS::Utility::Comparator
{
	struct StringEqualTo
	{
		bool operator()(KxDynamicStringRefW v1, KxDynamicStringRefW v2) const
		{
			return IsEqual(v1, v2);
		}
	};
	struct StringEqualToNoCase
	{
		bool operator()(KxDynamicStringRefW v1, KxDynamicStringRefW v2) const
		{
			return IsEqualNoCase(v1, v2);
		}
	};
}
