/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	template<class T> struct IsEnumCastOpAllowed: std::false_type {};
	template<class T> inline constexpr bool IsEnumCastOpAllowedV = IsEnumCastOpAllowed<T>::value;

	template<class T> struct IsEnumBitwiseOpAllowed: std::false_type {};
	template<class T> inline constexpr bool IsEnumBitwiseOpAllowedV = IsEnumBitwiseOpAllowed<T>::value;

	#define KxVFS_AllowEnumCastOp(T) template<> struct KxVFS::IsEnumCastOpAllowed<T>: std::true_type {}
	#define KxVFS_AllowEnumBitwiseOp(T) template<> struct KxVFS::IsEnumBitwiseOpAllowed<T>: std::true_type {}
}

namespace KxVFS
{
	namespace Internal
	{
		template<class T> inline constexpr bool IntCastAllowed = IsEnumCastOpAllowedV<T> || IsEnumBitwiseOpAllowedV<T>;
	}

	template<class TEnum>
	constexpr std::enable_if_t<Internal::IntCastAllowed<TEnum>, bool> ToBool(TEnum value)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<Tint>(value) != 0;
	}

	template<class Tint, class TEnum>
	constexpr std::enable_if_t<Internal::IntCastAllowed<TEnum>, Tint> ToInt(TEnum value)
	{
		return static_cast<Tint>(value);
	}
	template<class TEnum, class Tint = std::underlying_type_t<TEnum>>
	constexpr std::enable_if_t<Internal::IntCastAllowed<TEnum>, Tint> ToInt(TEnum value)
	{
		return static_cast<Tint>(value);
	}

	template<class TEnum, class TInt>
	constexpr std::enable_if_t<Internal::IntCastAllowed<TEnum>, TEnum> FromInt(TInt value)
	{
		return static_cast<TEnum>(value);
	}
}

namespace KxVFS
{
	template<class TEnum>
	constexpr std::enable_if_t<IsEnumBitwiseOpAllowedV<TEnum>, TEnum> operator&(TEnum left, TEnum right)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(static_cast<Tint>(left) & static_cast<Tint>(right));
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumBitwiseOpAllowedV<TEnum>, TEnum> operator|(TEnum left, TEnum right)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(static_cast<Tint>(left) | static_cast<Tint>(right));
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumBitwiseOpAllowedV<TEnum>, TEnum&> operator&=(TEnum& left, TEnum right)
	{
		left = left & right;
		return left;
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumBitwiseOpAllowedV<TEnum>, TEnum&> operator|=(TEnum& left, TEnum right)
	{
		left = left | right;
		return left;
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsEnumBitwiseOpAllowedV<TEnum>, TEnum> operator~(TEnum value)
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(~static_cast<Tint>(value));
	}
}
