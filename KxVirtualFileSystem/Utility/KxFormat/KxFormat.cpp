/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxFormat.h"

namespace KxVFS
{
	void KxFormatBase::ReplaceNext(KxDynamicStringRefW string)
	{
		ReplaceAnchor(string, m_CurrentArgument);
		m_CurrentArgument++;
	}
	void KxFormatBase::ReplaceAnchor(KxDynamicStringRefW string, size_t index, size_t startAt)
	{
		wchar_t indexBuffer[64] = {0};
		int indexLength = swprintf_s(indexBuffer, L"%%%zu", index);

		if (indexLength > 0)
		{
			KxDynamicStringRefW indexString(indexBuffer, indexLength);

			bool ok = false;
			size_t count = 0;
			size_t next = 0;

			do
			{
				ok = DoReplace(string, indexString, startAt + next, next);
				count++;
			}
			while (ok && count < 1024);
		}
	}
	bool KxFormatBase::DoReplace(KxDynamicStringRefW string, KxDynamicStringRefW index, size_t startAt, size_t& next)
	{
		size_t pos = m_String.find(index.data(), startAt);
		if (pos != KxDynamicStringW::npos && m_String.length() > pos + 1)
		{
			m_String.replace(pos, index.length(), string);
			next = pos + string.length();
			return true;
		}
		return false;
	}

	void KxFormatBase::FormatString(KxDynamicStringRefW arg, int fieldWidth, wchar_t fillChar)
	{
		if (fieldWidth == 0 || arg.length() >= (size_t)std::abs(fieldWidth))
		{
			ReplaceNext(arg);
		}
		else if (fieldWidth > 0)
		{
			KxDynamicStringW copy;
			copy.resize((size_t)fieldWidth - arg.length(), fillChar);
			copy += arg;
			ReplaceNext(copy);
		}
		else
		{
			KxDynamicStringW copy(arg);
			copy.append(static_cast<size_t>(-fieldWidth) - arg.length(), fillChar);
			ReplaceNext(copy);
		}
	}
	void KxFormatBase::FormatChar(wchar_t arg, int fieldWidth, wchar_t fillChar)
	{
		FormatString(KxDynamicStringW(&arg, 1), fieldWidth, fillChar);
	}

	void KxFormatBase::FormatPointer(const void* arg, int fieldWidth, wchar_t fillChar)
	{
		const size_t value = reinterpret_cast<size_t>(const_cast<void*>(arg));
		FormatString(FormatIntWithBase(value, 16, m_IsUpperCase), fieldWidth, fillChar);
	}
	void KxFormatBase::FormatBool(bool arg, int fieldWidth, wchar_t fillChar)
	{
		constexpr wchar_t ms_TrueU[] = L"TRUE";
		constexpr wchar_t ms_FalseU[] = L"FALSE";

		constexpr wchar_t ms_TrueL[] = L"true";
		constexpr wchar_t ms_FalseL[] = L"false";

		if (m_IsUpperCase)
		{
			FormatString(arg ? ms_TrueU : ms_FalseU, fieldWidth, fillChar);
		}
		else
		{
			FormatString(arg ? ms_TrueL : ms_FalseL, fieldWidth, fillChar);
		}
	}
	void KxFormatBase::FormatDouble(double arg, int precision, int fieldWidth, wchar_t format, wchar_t fillChar)
	{
		switch (format)
		{
			case L'g':
			case L'G':
			case L'e':
			case L'E':
			case L'f':
			case L'F':
			{
				break;
			}
			default:
			{
				return;
			}
		};

		wchar_t formatString[64] = {0};
		if (precision >= 0)
		{
			swprintf_s(formatString, L"%%.%d%c", precision, (wchar_t)format);
		}
		else
		{
			swprintf_s(formatString, L"%%%c", (wchar_t)format);
		}
		FormatString(KxDynamicStringW::Format(formatString, arg), fieldWidth, fieldWidth);
	}
}

#if 0
static void TestFunction()
{
	using namespace KxVFS;

	KxFormat format(L"%1 %2 %3");

	format(true);
	format(false);
	format(UCHAR(1));

	KxDynamicStringW strDyn;
	std::string strStd;
	std::wstring strStdW;
	std::string_view strStdV;
	std::wstring_view strStdWV;

	format(strDyn);
	format(strStd);
	format(strStdW);
	format(strStdV);
	format(strStdWV);

	format("a");
	format(L"a");
	format('a');
	format(L'a');

	format(wchar_t('a'));
	format(strDyn[0]);

	format(1);
	format(1u);

	format(1i8);
	format(1ui8);

	format(1i16);
	format(1ui16);

	format(1i32);
	format(1ui32);

	format(1i64);
	format(1ui64);

	void* p1 = nullptr;
	const void* p2 = nullptr;
	format(p1);
	format(p2);

	int* pi1 = nullptr;
	const int* pi2 = nullptr;
	format(pi1);
	format(pi2);

	const char* c1 = nullptr;
	char* c2 = nullptr;
	format(c1);
	format(c2);

	const wchar_t* wc1 = nullptr;
	wchar_t* wc2 = nullptr;
	format(wc1);
	format(wc2);

	format(7.0);
	format(2.7f);

	enum e1
	{
		e1_value,
	};
	enum class e2
	{
		e2_value,
	};

	format(e1::e1_value);
	format(e2::e2_value);

	struct Integer
	{
		int a = 42;

		operator int() const
		{
			return a;
		}
	};
	format(Integer());
}
#endif
