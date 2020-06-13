#pragma once
#include "KxVFS/Utility.h"
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

namespace KxVFS::Utility::Comparator::Private
{
	enum class CompareResult: int
	{
		LessThan = CSTR_LESS_THAN,
		Equal = CSTR_EQUAL,
		GreaterThan = CSTR_GREATER_THAN,
	};

	inline CompareResult DoCompareStrings(DynamicStringRefW left, DynamicStringRefW right, bool ignoreCase) noexcept
	{
		return static_cast<CompareResult>(::CompareStringOrdinal(left.data(), static_cast<int>(left.length()), right.data(), static_cast<int>(right.length()), ignoreCase));
	}
	inline CompareResult CompareStrings(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return DoCompareStrings(left, right, false);
	}
	inline CompareResult CompareStringsNoCase(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return DoCompareStrings(left, right, true);
	}
}

namespace KxVFS::Utility::Comparator
{
	// ==
	template<class T>
	bool IsEqual(const T& left, const T& right)
	{
		return left == right;
	}
	
	inline bool IsEqual(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStrings(left, right) == Private::CompareResult::Equal;
	}
	inline bool IsEqualNoCase(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStringsNoCase(left, right) == Private::CompareResult::Equal;
	}

	// <
	template<class T>
	bool IsLess(const T& left, const T& right)
	{
		return left < right;
	}
	
	inline bool IsLess(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStrings(left, right) == Private::CompareResult::LessThan;
	}
	inline bool IsLessNoCase(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStringsNoCase(left, right) == Private::CompareResult::LessThan;
	}

	// >
	template<class T>
	bool IsGreater(const T& left, const T& right)
	{
		return left > right;
	}
	
	inline bool IsGreater(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStrings(left, right) == Private::CompareResult::GreaterThan;
	}
	inline bool IsGreaterNoCase(DynamicStringRefW left, DynamicStringRefW right) noexcept
	{
		return Private::CompareStringsNoCase(left, right) == Private::CompareResult::GreaterThan;
	}
}

namespace KxVFS::Utility::Comparator
{
	struct StringEqualTo
	{
		bool operator()(DynamicStringRefW left, DynamicStringRefW right) const noexcept
		{
			return IsEqual(left, right);
		}
	};
	struct StringEqualToNoCase
	{
		bool operator()(DynamicStringRefW left, DynamicStringRefW right) const noexcept
		{
			return IsEqualNoCase(left, right);
		}
	};

	struct StringLessThan
	{
		bool operator()(DynamicStringRefW left, DynamicStringRefW right) const noexcept
		{
			return IsLess(left, right);
		}
	};
	struct StringLessThanNoCase
	{
		bool operator()(DynamicStringRefW left, DynamicStringRefW right) const noexcept
		{
			return IsLessNoCase(left, right);
		}
	};

	struct StringHash
	{
		size_t operator()(DynamicStringRefW value) const noexcept
		{
			return std::hash<DynamicStringRefW>()(value);
		}
	};
	struct StringHashNoCase
	{
		// From Boost
		template<class T>
		static void hash_combine(size_t& seed, const T& v) noexcept
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + size_t(0x9e3779b9u) + (seed << 6) + (seed >> 2);
		}

		size_t operator()(DynamicStringRefW value) const noexcept
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
	template<class TValue>
	using Map = std::map<DynamicStringW, TValue, StringLessThan>;
	
	template<class TValue>
	using MapNoCase = std::map<DynamicStringW, TValue, StringLessThanNoCase>;

	template<class TValue>
	using UnorderedMap = std::unordered_map<DynamicStringW, TValue, StringHash, StringEqualTo>;
	
	template<class TValue>
	using UnorderedMapNoCase = std::unordered_map<DynamicStringW, TValue, StringHashNoCase, StringEqualToNoCase>;

	using Set = std::set<DynamicStringW, StringLessThan>;
	using SetNoCase = std::set<DynamicStringW, StringLessThanNoCase>;

	using UnorderedSet = std::unordered_set<DynamicStringW, StringHash, StringEqualTo>;
	using UnorderedSetNoCase = std::unordered_set<DynamicStringW, StringHashNoCase, StringEqualToNoCase>;
}
