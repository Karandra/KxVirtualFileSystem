/*
Copyright © 2020 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility::String
{
	template<class... Args>
	KxDynamicStringW Concat(Args&&... arg)
	{
		return (KxDynamicStringW(arg) + ...);
	}
	
	template<class... Args>
	KxDynamicStringW ConcatWithSeparator(KxDynamicStringRefW sep, Args&&... arg)
	{
		if (!sep.empty())
		{
			KxDynamicStringW value = ((KxDynamicStringW(arg) + sep) + ...);
			value.erase(value.length() - sep.length() - 1, sep.length());
			return value;
		}
		return Concat(std::forward<Args>(arg)...);
	}

	template<class TFunctor>
	size_t SplitBySeparator(KxDynamicStringRefW string, wchar_t sep, TFunctor&& func)
	{
		size_t separatorPos = string.find(sep);
		if (separatorPos == KxDynamicStringRefW::npos)
		{
			func(string);
			return 1;
		}

		size_t pos = 0;
		size_t count = 0;
		while (pos < string.length() && separatorPos <= string.length())
		{
			KxDynamicStringRefW stringPiece = string.substr(pos, separatorPos - pos);
			const size_t stringPieceLength = stringPiece.length();

			if (!stringPiece.empty())
			{
				count++;
				if (!func(std::move(stringPiece)))
				{
					return count;
				}
			}

			pos += stringPieceLength + 1;
			separatorPos = string.find(sep, pos);

			// No separator found, but this is not the last element
			if (separatorPos == KxDynamicStringRefW::npos && pos < string.length())
			{
				separatorPos = string.length();
			}
		}
		return count;
	}
	
	template<class TFunctor>
	size_t SplitBySeparator(KxDynamicStringRefW string, KxDynamicStringRefW sep, TFunctor&& func)
	{
		if (sep.empty() && !string.empty())
		{
			func(string);
			return 1;
		}

		size_t separatorPos = string.find(sep);
		if (separatorPos == KxDynamicStringRefW::npos)
		{
			func(string);
			return 1;
		}

		size_t pos = 0;
		size_t count = 0;
		while (pos < string.length() && separatorPos <= string.length())
		{
			KxDynamicStringRefW stringPiece = string.substr(pos, separatorPos - pos);
			const size_t stringPieceLength = stringPiece.length();

			if (!stringPiece.empty())
			{
				count++;
				if (!func(std::move(stringPiece)))
				{
					return count;
				}
			}

			pos += stringPieceLength + sep.length();
			separatorPos = string.find(sep, pos);

			// No separator found, but this is not the last element
			if (separatorPos == KxDynamicStringRefW::npos && pos < string.length())
			{
				separatorPos = string.length();
			}
		}
		return count;
	}

	template<class TFunctor>
	size_t SplitByLength(KxDynamicStringRefW string, size_t length, TFunctor&& func)
	{
		const size_t stringLength = string.length();

		if (length != 0)
		{
			size_t count = 0;
			for (size_t i = 0; i < stringLength; i += length)
			{
				KxDynamicStringRefW stringPiece = string.substr(i, length);
				if (!stringPiece.empty())
				{
					count++;
					if (!func(std::move(stringPiece)))
					{
						return count;
					}
				}
			}
			return count;
		}
		else
		{
			func(string);
			return 1;
		}
		return 0;
	}
}
