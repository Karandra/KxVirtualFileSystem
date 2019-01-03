/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxFramework. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/Utility.h"
#include <unordered_set>
#include <unordered_map>

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

	struct StringHash
	{
		size_t operator()(KxDynamicStringRefW value) const
		{
			return std::hash<KxDynamicStringRefW>()(value);
		}
	};
	struct StringHashOnCase
	{
		// From Boost
		template<class T> static void hash_combine(size_t& seed, const T& v)
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
		}

		size_t operator()(KxDynamicStringRefW value) const
		{
			size_t hashValue = 0;
			for (wchar_t c: value)
			{
				hash_combine(hashValue, Utility::CharToLower(c));
			}
			return hashValue;
		}
	};
}

namespace KxVFS::Utility::Comparator
{
	template<class TValue> using UnorderedMap = std::unordered_map<KxDynamicStringW, TValue, StringHash, StringEqualTo>;
	template<class TValue> using UnorderedMapNoCase = std::unordered_map<KxDynamicStringW, TValue, StringHashOnCase, StringEqualToNoCase>;

	using UnorderedSet = std::unordered_set<KxDynamicStringW, StringHash, StringEqualTo>;
	using UnorderedSetNoCase = std::unordered_set<KxDynamicStringW, StringHashOnCase, StringEqualToNoCase>;
}
