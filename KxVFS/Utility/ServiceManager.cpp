#include "stdafx.h"
#include "KxVFS/Utility.h"
#include "ServiceManager.h"

namespace KxVFS
{
	bool ServiceManager::Open(FlagSet<ServiceManagerAccess> accessMode) noexcept
	{
		m_Handle = ::OpenSCManagerW(nullptr, nullptr, accessMode.ToInt());
		return !IsNull();
	}
}
