#include "stdafx.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ServiceManager.h"

namespace KxVFS
{
	bool ServiceManager::Open(ServiceManagerAccess accessMode)
	{
		m_Handle = ::OpenSCManagerW(nullptr, nullptr, ToInt(accessMode));
		return IsOK();
	}
}
