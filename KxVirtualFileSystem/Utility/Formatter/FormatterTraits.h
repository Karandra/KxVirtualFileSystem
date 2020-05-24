#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility/DynamicString/DynamicString.h"

namespace KxVFS
{
	template<class Type>
	class FormatterTraits
	{
		public:
			using TInitial = typename Type;
			using TDecayed = typename std::decay<TInitial>::type;

		protected:
			template<class T, class... Args>
			constexpr static bool IsConvertibleToAnyOf()
			{
				return std::disjunction_v<std::is_convertible<T, Args>...>;
			}

		public:
			constexpr static bool IsConst()
			{
				return std::is_const_v<TInitial>;
			}
			constexpr static bool IsReference()
			{
				return std::is_reference_v<TInitial>;
			}

			constexpr static bool IsChar()
			{
				return std::is_same_v<TDecayed, char>;
			}
			constexpr static bool IsWChar()
			{
				return std::is_same_v<TDecayed, wchar_t>;
			}
			
			constexpr static bool IsCharPointer()
			{
				return std::is_same_v<TDecayed, char*> || std::is_same_v<TDecayed, const char*>;
			}
			constexpr static bool IsWCharPointer()
			{
				return std::is_same_v<TDecayed, wchar_t*> || std::is_same_v<TDecayed, const wchar_t*>;
			}

			constexpr static bool IsCharArray()
			{
				return IsArray() && IsCharPointer();
			}
			constexpr static bool IsWCharArray()
			{
				return IsArray() && IsWCharPointer();
			}

			constexpr static bool IsArray()
			{
				return std::is_array_v<TInitial>;
			}
			constexpr static bool IsBool()
			{
				return std::is_same_v<TDecayed, bool>;
			}
			constexpr static bool IsInteger()
			{
				return std::is_integral_v<TDecayed> || std::is_enum_v<TDecayed>;
			}
			constexpr static bool IsEnum()
			{
				return std::is_enum_v<TDecayed>;
			}
			constexpr static bool IsFloat()
			{
				return std::is_floating_point_v<TDecayed>;
			}
			constexpr static bool IsPointer()
			{
				return std::is_pointer_v<TDecayed>;
			}

			constexpr static bool IsDynamicString()
			{
				return std::is_same_v<TDecayed, DynamicStringW>;
			}
			constexpr static bool IsDynamicStringRef()
			{
				return std::is_same_v<TDecayed, DynamicStringRefW>;
			}
			constexpr static bool IsStdWString()
			{
				return std::is_same_v<TDecayed, std::wstring>;
			}
			constexpr static bool IsConstructibleToDynamicString()
			{
				return std::is_constructible_v<DynamicStringW, TInitial>;
			}
			constexpr static bool IsConstructibleToWChar()
			{
				return std::is_constructible_v<wchar_t, TInitial>;
			}

			constexpr static bool IsConvertibleToAnyInt()
			{
				return IsConvertibleToAnyOf<TDecayed,
					int8_t, int16_t, int32_t, int64_t,
					uint8_t, uint16_t, uint32_t, uint64_t>();
			}
			constexpr static bool IsConvertibleToAnyFloat()
			{
				return IsConvertibleToAnyOf<TDecayed, float, double>();
			}

		public:
			constexpr static bool FmtString()
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return IsWChar() || IsWCharPointer() || IsWCharArray() || IsBool() || IsDynamicStringRef() || IsConstructibleToDynamicString() || IsStdWString();
				}
				return false;
			}
			constexpr static bool FmtInteger()
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return (IsInteger() && !(IsBool() || IsWChar() || IsPointer()));
				}
				return false;
			}
			constexpr static bool FmtFloat()
			{
				if (!IsChar() && !IsCharPointer() && !IsCharArray())
				{
					return IsFloat();
				}
				return false;
			}
			constexpr static bool FmtPointer()
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
			constexpr static int StringFiledWidth()
			{
				return 0;
			}
			constexpr static wchar_t StringFillChar()
			{
				return L' ';
			}

			constexpr static int IntFiledWidth()
			{
				return 0;
			}
			constexpr static int IntBase()
			{
				return 10;
			}
			constexpr static wchar_t IntFillChar()
			{
				return L' ';
			}

			constexpr static int PtrFiledWidth()
			{
				return sizeof(void*) * 2;
			}
			constexpr static wchar_t PtrFillChar()
			{
				return L'0';
			}

			constexpr static int FloatFiledWidth()
			{
				return 0;
			}
			constexpr static int FloatPrecision()
			{
				return -1;
			}
			constexpr static wchar_t FloatFormat()
			{
				return L'f';
			}
			constexpr static wchar_t FloatFillChar()
			{
				return L'0';
			}
	};
}
