/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxIFileFinder.h"

namespace KxVFS
{
	KxDynamicStringW KxIFileFinder::Normalize(KxDynamicStringRefW source, bool start, bool end) const
	{
		return Utility::NormalizeFilePath(source);
	}
	KxDynamicStringW KxIFileFinder::ConstructSearchQuery(KxDynamicStringRefW source, KxDynamicStringRefW filter) const
	{
		if (!filter.empty())
		{
			KxDynamicStringW out = source;
			out += L'\\';
			out += filter;

			return out;
		}
		return source;
	}
	KxDynamicStringW KxIFileFinder::ExtrctSourceFromSearchQuery(KxDynamicStringRefW searchQuery) const
	{
		KxDynamicStringW source = KxDynamicStringW(searchQuery).before_last(L'\\');
		return source.empty() ? searchQuery : source;
	}
}
