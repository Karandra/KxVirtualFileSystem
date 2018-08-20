/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxDynamicString.h"

const KxDynamicString KxNullDynamicStrig;
const KxDynamicStringRef KxNullDynamicStrigRef;

KxDynamicString KxDynamicString::Format(const CharT* formatString, ...)
{
	KxDynamicString buffer;

	va_list argptr;
	va_start(argptr, formatString);
	int count = _vscwprintf(formatString, argptr);
	if (count > 0)
	{
		buffer.resize((size_t)count);
		::vswprintf(buffer.data(), buffer.size(), formatString, argptr);
	}
	va_end(argptr);
	return buffer;
}
