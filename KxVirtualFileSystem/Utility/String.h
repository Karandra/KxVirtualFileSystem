#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility::String
{
	template<class... Args>
	DynamicStringW Concat(Args&&... arg)
	{
		return (DynamicStringW(arg) + ...);
	}
	
	template<class... Args>
	DynamicStringW ConcatWithSeparator(DynamicStringRefW sep, Args&&... arg)
	{
		if (!sep.empty())
		{
			DynamicStringW value = ((DynamicStringW(arg) + sep) + ...);
			value.erase(value.length() - sep.length() - 1, sep.length());
			return value;
		}
		return Concat(std::forward<Args>(arg)...);
	}

	template<class TFunctor>
	size_t SplitBySeparator(DynamicStringRefW string, wchar_t sep, TFunctor&& func)
	{
		size_t separatorPos = string.find(sep);
		if (separatorPos == DynamicStringRefW::npos)
		{
			func(string);
			return 1;
		}

		size_t pos = 0;
		size_t count = 0;
		while (pos < string.length() && separatorPos <= string.length())
		{
			DynamicStringRefW stringPiece = string.substr(pos, separatorPos - pos);
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
			if (separatorPos == DynamicStringRefW::npos && pos < string.length())
			{
				separatorPos = string.length();
			}
		}
		return count;
	}
	
	template<class TFunctor>
	size_t SplitBySeparator(DynamicStringRefW string, DynamicStringRefW sep, TFunctor&& func)
	{
		if (sep.empty() && !string.empty())
		{
			func(string);
			return 1;
		}

		size_t separatorPos = string.find(sep);
		if (separatorPos == DynamicStringRefW::npos)
		{
			func(string);
			return 1;
		}

		size_t pos = 0;
		size_t count = 0;
		while (pos < string.length() && separatorPos <= string.length())
		{
			DynamicStringRefW stringPiece = string.substr(pos, separatorPos - pos);
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
			if (separatorPos == DynamicStringRefW::npos && pos < string.length())
			{
				separatorPos = string.length();
			}
		}
		return count;
	}

	template<class TFunctor>
	size_t SplitByLength(DynamicStringRefW string, size_t length, TFunctor&& func)
	{
		const size_t stringLength = string.length();

		if (length != 0)
		{
			size_t count = 0;
			for (size_t i = 0; i < stringLength; i += length)
			{
				DynamicStringRefW stringPiece = string.substr(i, length);
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
