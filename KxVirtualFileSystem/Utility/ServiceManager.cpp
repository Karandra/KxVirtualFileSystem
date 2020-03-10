/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
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
