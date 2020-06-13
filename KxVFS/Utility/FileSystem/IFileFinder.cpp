#include "stdafx.h"
#include "KxVFS/Utility.h"
#include "IFileFinder.h"

namespace KxVFS
{
	DynamicStringW IFileFinder::Normalize(DynamicStringRefW source, bool start, bool end) const
	{
		return Utility::NormalizeFilePath(source);
	}
	DynamicStringW IFileFinder::ConstructSearchQuery(DynamicStringRefW source, DynamicStringRefW filter) const
	{
		if (!filter.empty())
		{
			DynamicStringW out = source;
			out += L'\\';
			out += filter;

			return out;
		}
		return source;
	}
	DynamicStringW IFileFinder::ExtrctSourceFromSearchQuery(DynamicStringRefW searchQuery) const
	{
		DynamicStringW source = DynamicStringW(searchQuery).before_last(L'\\');
		return source.empty() ? searchQuery : source;
	}
}
