/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	template<class T> struct IsEnumClass: std::false_type
	{
	};
	template<class T> inline constexpr bool IsEnumClassV = IsEnumClass<T>::value;

	#define KxVFSDeclareEnumOperations(T) template<> struct KxVFS::IsEnumClass<std::decay_t<T>>: public std::true_type {}
}

namespace KxVFS
{
	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, bool> ToBool(TEnum value)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<Tint>(value) != 0;
	}

	template<class Tint, class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, Tint> ToInt(TEnum value)
	{
		return static_cast<Tint>(value);
	}
	template<class TEnum, class Tint = std::underlying_type_t<TEnum>>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, Tint> ToInt(TEnum value)
	{
		return static_cast<Tint>(value);
	}

	template<class TEnum, class TInt>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum> FromInt(TInt value)
	{
		return static_cast<TEnum>(value);
	}
}

namespace KxVFS
{
	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum> operator&(TEnum left, TEnum right)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(static_cast<Tint>(left) & static_cast<Tint>(right));
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum> operator|(TEnum left, TEnum right)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(static_cast<Tint>(left) | static_cast<Tint>(right));
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum&> operator&=(TEnum& left, TEnum right)
	{
		left = left & right;
		return left;
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum&> operator|=(TEnum& left, TEnum right)
	{
		left = left | right;
		return left;
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumClassV<TEnum>, TEnum> operator~(TEnum value)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(~static_cast<Tint>(value));
	}
}
