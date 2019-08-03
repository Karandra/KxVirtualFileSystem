/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxFormatTraits.h"

namespace KxVFS
{
	class KxVFS_API KxFormatBase
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

			void ReplaceNext(KxDynamicStringRefW string);
			void ReplaceAnchor(KxDynamicStringRefW string, size_t index, size_t startAt = 0);
			bool DoReplace(KxDynamicStringRefW string, KxDynamicStringRefW index, size_t startAt, size_t& next);

			void FormatString(KxDynamicStringRefW arg, int fieldWidth, wchar_t fillChar);
			void FormatChar(wchar_t arg, int fieldWidth, wchar_t fillChar);
			void FormatBool(bool arg, int fieldWidth, wchar_t fillChar);
			void FormatDouble(double arg, int precision, int fieldWidth, wchar_t format, wchar_t fillChar);
			void FormatPointer(const void* arg, int fieldWidth, wchar_t fillChar);

			template<class T> static KxDynamicStringW FormatIntWithBase(T value, int base = 10, bool upper = false)
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

				KxDynamicStringW result;
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
			KxFormatBase(const KxDynamicStringW& format)
				:m_String(format)
			{
			}
			virtual ~KxFormatBase() = default;

		public:
			KxDynamicStringW ToString() const
			{
				return KxDynamicStringRefW(m_String);
			}
			operator KxDynamicStringW() const
			{
				return KxDynamicStringRefW(m_String);
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
			KxFormatBase& UpperCase(bool value = true)
			{
				m_IsUpperCase = value;
				return *this;
			}
			KxFormatBase& LowerCase(bool value = true)
			{
				m_IsUpperCase = !value;
				return *this;
			}
	};

	template<class FmtTraits = KxFormatTraits> class KxFormat: public KxFormatBase
	{
		public:
			using FormatTraits = typename FmtTraits;
			template<class T> using TypeTraits = KxFormatTypeTraits<T>;

		protected:
			template<class T> void FormatInt(T&& arg, int fieldWidth, int base, wchar_t fillChar)
			{
				FormatString(FormatIntWithBase(arg, base, IsUpperCase()), fieldWidth, fillChar);
			}

		public:
			KxFormat(const KxDynamicStringW& format)
				:KxFormatBase(format)
			{
			}

		public:
			KxFormat& UpperCase(bool value = true)
			{
				KxFormatBase::UpperCase(value);
				return *this;
			}
			KxFormat& LowerCase(bool value = true)
			{
				KxFormatBase::LowerCase(value);
				return *this;
			}

		public:
			template<class T> typename std::enable_if<TypeTraits<T>::FmtString(), KxFormat&>::type
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
					FormatString(KxDynamicStringRefW(arg.data(), arg.size()), fieldWidth, fillChar);
				}
				else
				{
					static_assert(false, "KxFormat: unsupported type for string formatting");
				}
				return *this;
			}
		
			template<class T> typename std::enable_if<TypeTraits<T>::FmtInteger(), KxFormat&>::type
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

			template<class T> typename std::enable_if<TypeTraits<T>::FmtPointer(), KxFormat&>::type
			operator()(T arg,
					   int fieldWidth = FormatTraits::PtrFiledWidth(),
					   wchar_t fillChar = FormatTraits::PtrFillChar())
			{
				FormatPointer(arg, fieldWidth, fillChar);
				return *this;
			}

			template<class T> typename std::enable_if<TypeTraits<T>::FmtFloat(), KxFormat&>::type
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
