#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility/DynamicString/DynamicString.h"

namespace KxVFS
{
	template<class Type>
	class FormatterTypeTraits
	{
		public:
			using TInitial = typename Type;
			using TDecayed = typename std::decay<TInitial>::type;

		protected:
			template<class T, class... Args>
			constexpr static bool IsConvertibleToAnyOf() noexcept
			{
				return std::disjunction_v<std::is_convertible<T, Args>...>;
			}

		public:
			constexpr static bool IsConst() noexcept
			{
				return std::is_const_v<TInitial>;
			}
			constexpr static bool IsReference() noexcept
			{
				return std::is_reference_v<TInitial>;
			}

			constexpr static bool IsChar() noexcept
			{
				return std::is_same_v<TDecayed, char>;
			}
			constexpr static bool IsWChar() noexcept
			{
				return std::is_same_v<TDecayed, wchar_t>;
			}
			
			constexpr static bool IsCharPointer() noexcept
			{
				return std::is_same_v<TDecayed, char*> || std::is_same_v<TDecayed, const char*>;
			}
			constexpr static bool IsWCharPointer() noexcept
			{
				return std::is_same_v<TDecayed, wchar_t*> || std::is_same_v<TDecayed, const wchar_t*>;
			}

			constexpr static bool IsCharArray() noexcept
			{
				return IsArray() && IsCharPointer();
			}
			constexpr static bool IsWCharArray() noexcept
			{
				return IsArray() && IsWCharPointer();
			}

			constexpr static bool IsArray() noexcept
			{
				return std::is_array_v<TInitial>;
			}
			constexpr static bool IsBool() noexcept
			{
				return std::is_same_v<TDecayed, bool>;
			}
			constexpr static bool IsInteger() noexcept
			{
				return std::is_integral_v<TDecayed> || std::is_enum_v<TDecayed>;
			}
			constexpr static bool IsEnum() noexcept
			{
				return std::is_enum_v<TDecayed>;
			}
			constexpr static bool IsFloat() noexcept
			{
				return std::is_floating_point_v<TDecayed>;
			}
			constexpr static bool IsPointer() noexcept
			{
				return std::is_pointer_v<TDecayed>;
			}

			constexpr static bool IsDynamicString() noexcept
			{
				return std::is_same_v<TDecayed, DynamicStringW>;
			}
			constexpr static bool IsDynamicStringRef() noexcept
			{
				return std::is_same_v<TDecayed, DynamicStringRefW>;
			}
			constexpr static bool IsStdWString() noexcept
			{
				return std::is_same_v<TDecayed, std::wstring>;
			}
			constexpr static bool IsConstructibleToDynamicString() noexcept
			{
				return std::is_constructible_v<DynamicStringW, TInitial>;
			}
			constexpr static bool IsConstructibleToWChar() noexcept
			{
				return std::is_constructible_v<wchar_t, TInitial>;
			}

			constexpr static bool IsConvertibleToAnyInt() noexcept
			{
				return IsConvertibleToAnyOf<TDecayed,
					int8_t, int16_t, int32_t, int64_t,
					uint8_t, uint16_t, uint32_t, uint64_t>();
			}
			constexpr static bool IsConvertibleToAnyFloat() noexcept
			{
				return IsConvertibleToAnyOf<TDecayed, float, double>();
			}

		public:
			constexpr static bool FmtString() noexcept
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return IsWChar() || IsWCharPointer() || IsWCharArray() || IsBool() || IsDynamicStringRef() || IsConstructibleToDynamicString() || IsStdWString();
				}
				return false;
			}
			constexpr static bool FmtInteger() noexcept
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return (IsInteger() && !(IsBool() || IsWChar() || IsPointer()));
				}
				return false;
			}
			constexpr static bool FmtFloat() noexcept
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return IsFloat();
				}
				return false;
			}
			constexpr static bool FmtPointer() noexcept
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return IsPointer() && !(IsWCharPointer() || IsWCharArray());
				}
				return false;
			}
	};
}

namespace KxVFS
{
	class FormatterDefaultTraits
	{
		public:
			constexpr static int StringFiledWidth() noexcept
			{
				return 0;
			}
			constexpr static wchar_t StringFillChar() noexcept
			{
				return L' ';
			}

			constexpr static int IntFiledWidth() noexcept
			{
				return 0;
			}
			constexpr static int IntBase() noexcept
			{
				return 10;
			}
			constexpr static wchar_t IntFillChar() noexcept
			{
				return L' ';
			}

			constexpr static int PtrFiledWidth() noexcept
			{
				return sizeof(void*) * 2;
			}
			constexpr static wchar_t PtrFillChar() noexcept
			{
				return L'0';
			}

			constexpr static int FloatFiledWidth() noexcept
			{
				return 0;
			}
			constexpr static int FloatPrecision() noexcept
			{
				return -1;
			}
			constexpr static wchar_t FloatFormat() noexcept
			{
				return L'f';
			}
			constexpr static wchar_t FloatFillChar() noexcept
			{
				return L'0';
			}
	};
}
