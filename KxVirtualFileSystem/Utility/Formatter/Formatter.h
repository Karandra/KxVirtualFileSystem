#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "FormatterTraits.h"

namespace KxVFS
{
	class KxVFS_API FormatterBase
	{
		private:
			std::wstring m_String;
			size_t m_CurrentArgument = 1;
			bool m_IsUpperCase = false;

		protected:
			const std::wstring& GetString() const
			{
				return m_String;
			}
			std::wstring& GetString()
			{
				return m_String;
			}

			void ReplaceNext(DynamicStringRefW string);
			void ReplaceAnchor(DynamicStringRefW string, size_t index, size_t startAt = 0);
			bool DoReplace(DynamicStringRefW string, DynamicStringRefW index, size_t startAt, size_t& next);

			void FormatString(DynamicStringRefW arg, int fieldWidth, wchar_t fillChar);
			void FormatChar(wchar_t arg, int fieldWidth, wchar_t fillChar);
			void FormatBool(bool arg, int fieldWidth, wchar_t fillChar);
			void FormatDouble(double arg, int precision, int fieldWidth, wchar_t format, wchar_t fillChar);
			void FormatPointer(const void* arg, int fieldWidth, wchar_t fillChar);

			template<class T> static DynamicStringW FormatIntWithBase(T value, int base = 10, bool upper = false)
			{
				static_assert(std::is_integral_v<T>);

				constexpr wchar_t digitsL[] = L"0123456789abcdefghijklmnopqrstuvwxyz";
				constexpr wchar_t digitsU[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
				const wchar_t* digits = upper ? digitsU : digitsL;

				// Make positive
				bool isNegative = false;
				if constexpr(std::is_signed<T>::value)
				{
					if (value < 0)
					{
						isNegative = true;
						value = -value;
					}
				}

				DynamicStringW result;
				if (base >= 2 && base <= 36)
				{
					do
					{
						result += digits[value % base];
						value /= base;
					}
					while (value);

					if (isNegative)
					{
						result += L'-';
					}
					std::reverse(result.begin(), result.end());
				}
				return result;
			};

		public:
			FormatterBase(const DynamicStringW& format)
				:m_String(format)
			{
			}
			virtual ~FormatterBase() = default;

		public:
			DynamicStringW ToString() const
			{
				return DynamicStringRefW(m_String);
			}
			operator DynamicStringW() const
			{
				return DynamicStringRefW(m_String);
			}

			size_t GetCurrentArgumentIndex() const
			{
				return m_CurrentArgument;
			}
			void SetCurrentArgumentIndex(size_t index)
			{
				m_CurrentArgument = index;
			}
		
			bool IsUpperCase() const
			{
				return m_IsUpperCase;
			}
			FormatterBase& UpperCase(bool value = true)
			{
				m_IsUpperCase = value;
				return *this;
			}
			FormatterBase& LowerCase(bool value = true)
			{
				m_IsUpperCase = !value;
				return *this;
			}
	};

	template<class FmtTraits = FormatterDefaultTraits>
	class Formatter: public FormatterBase
	{
		public:
			using FormatTraits = typename FmtTraits;
			
			template<class T>
			using TypeTraits = FormatterTypeTraits<T>;

		protected:
			template<class T> void FormatInt(T&& arg, int fieldWidth, int base, wchar_t fillChar)
			{
				FormatString(FormatIntWithBase(arg, base, IsUpperCase()), fieldWidth, fillChar);
			}

		public:
			Formatter(const DynamicStringW& format)
				:FormatterBase(format)
			{
			}

		public:
			Formatter& UpperCase(bool value = true)
			{
				FormatterBase::UpperCase(value);
				return *this;
			}
			Formatter& LowerCase(bool value = true)
			{
				FormatterBase::LowerCase(value);
				return *this;
			}

		public:
			template<class T> typename std::enable_if<TypeTraits<T>::FmtString(), Formatter&>::type
			operator()(const T& arg,
					   int fieldWidth = FormatTraits::StringFiledWidth(),
					   wchar_t fillChar = FormatTraits::StringFillChar())
			{
				TypeTraits<T> trait;
				if constexpr(trait.IsBool())
				{
					FormatBool(arg, fieldWidth, fillChar);
				}
				else if constexpr(trait.IsConstructibleToWChar())
				{
					FormatChar(arg, fieldWidth, fillChar);
				}
				else if constexpr(trait.IsConstructibleToDynamicString())
				{
					FormatString(arg, fieldWidth, fillChar);
				}
				else if constexpr(trait.IsDynamicStringRef() || trait.IsStdWString())
				{
					FormatString(DynamicStringRefW(arg.data(), arg.size()), fieldWidth, fillChar);
				}
				else
				{
					static_assert(false, "Formatter: unsupported type for string formatting");
				}
				return *this;
			}
		
			template<class T> typename std::enable_if<TypeTraits<T>::FmtInteger(), Formatter&>::type
			operator()(T arg,
					   int fieldWidth = FormatTraits::IntFiledWidth(),
					   int base = FormatTraits::IntBase(),
					   wchar_t fillChar = FormatTraits::IntFillChar())
			{
				if constexpr(TypeTraits<T>().IsEnum())
				{
					FormatInt(static_cast<std::underlying_type_t<T>>(arg), fieldWidth, base, fillChar);
				}
				else
				{
					FormatInt(arg, fieldWidth, base, fillChar);
				}
				return *this;
			}

			template<class T> typename std::enable_if<TypeTraits<T>::FmtPointer(), Formatter&>::type
			operator()(T arg,
					   int fieldWidth = FormatTraits::PtrFiledWidth(),
					   wchar_t fillChar = FormatTraits::PtrFillChar())
			{
				FormatPointer(arg, fieldWidth, fillChar);
				return *this;
			}

			template<class T> typename std::enable_if<TypeTraits<T>::FmtFloat(), Formatter&>::type
			operator()(T arg,
					   int precision = FormatTraits::FloatPrecision(),
					   int fieldWidth = FormatTraits::FloatFiledWidth(),
					   wchar_t format = FormatTraits::FloatFormat(),
					   wchar_t fillChar = FormatTraits::FloatFillChar())
			{
				FormatDouble(arg, precision, fieldWidth, format, fillChar);
				return *this;
			}
	};
}
