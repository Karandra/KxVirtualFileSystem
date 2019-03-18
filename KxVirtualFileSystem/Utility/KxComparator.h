/*
Copyright © 2019 Kerber. All rights reserved.

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

	inline CompareResult CompareStrings(KxDynamicStringRefW left, KxDynamicStringRefW right, bool ignoreCase)
	{
		return static_cast<CompareResult>(::CompareStringOrdinal(left.data(), static_cast<int>(left.length()), right.data(), static_cast<int>(right.length()), ignoreCase));
	}
	inline CompareResult CompareStrings(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return CompareStrings(left, right, false);
	}
	inline CompareResult CompareStringsNoCase(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return CompareStrings(left, right, true);
	}
}

namespace KxVFS::Utility::Comparator
{
	// ==
	template<class T> bool IsEqual(const T& left, const T& right)
	{
		return left == right;
	}
	inline bool IsEqual(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStrings(left, right) == Internal::CompareResult::Equal;
	}
	inline bool IsEqualNoCase(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStringsNoCase(left, right) == Internal::CompareResult::Equal;
	}

	// <
	template<class T> bool IsLess(const T& left, const T& right)
	{
		return left < right;
	}
	inline bool IsLess(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStrings(left, right) == Internal::CompareResult::LessThan;
	}
	inline bool IsLessNoCase(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStringsNoCase(left, right) == Internal::CompareResult::LessThan;
	}

	// >
	template<class T> bool IsGreater(const T& left, const T& right)
	{
		return left > right;
	}
	inline bool IsGreater(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStrings(left, right) == Internal::CompareResult::GreaterThan;
	}
	inline bool IsGreaterNoCase(KxDynamicStringRefW left, KxDynamicStringRefW right)
	{
		return Internal::CompareStringsNoCase(left, right) == Internal::CompareResult::GreaterThan;
	}
}

namespace KxVFS::Utility::Comparator
{
	struct StringEqualTo
	{
		bool operator()(KxDynamicStringRefW left, KxDynamicStringRefW right) const
		{
			return IsEqual(left, right);
		}
	};
	struct StringEqualToNoCase
	{
		bool operator()(KxDynamicStringRefW left, KxDynamicStringRefW right) const
		{
			return IsEqualNoCase(left, right);
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
