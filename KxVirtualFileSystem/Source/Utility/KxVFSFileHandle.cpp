#include "KxVirtualFileSystem.h"
#include "KxVFSFileHandle.h"
#include "KxDynamicString.h"

KxDynamicString KxVFSFileHandle::GetPath() const
{
	KxDynamicString sOut;
	DWORD nFlags = VOLUME_NAME_DOS|FILE_NAME_NORMALIZED;
	DWORD nLength = GetFinalPathNameByHandleW(m_Handle, NULL, 0, nFlags);
	if (nLength != 0)
	{
		sOut.reserve(nLength + 1u);
		GetFinalPathNameByHandleW(m_Handle, sOut.data(), (DWORD)sOut.length(), nFlags);
		sOut.erase(0, 4); // Remove "\\?\" prefix
	}
	return sOut;
}
