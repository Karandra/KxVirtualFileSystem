#include "KxVirtualFileSystem.h"
#include "KxDynamicString.h"

const KxDynamicString KxNullDynamicStrig;
const KxDynamicStringRef KxNullDynamicStrigRef;

KxDynamicString KxDynamicString::Format(const CharT* sFormatString, ...)
{
	KxDynamicString sBuffer;

	va_list argptr;
	va_start(argptr, sFormatString);
	int nCount = _vscwprintf(sFormatString, argptr);
	if (nCount > 0)
	{
		sBuffer.resize((size_t)nCount);
		::vswprintf(sBuffer.data(), sBuffer.size(), sFormatString, argptr);
	}
	va_end(argptr);
	return sBuffer;
}
