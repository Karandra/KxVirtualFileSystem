/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "ServiceConstants.h"
#include "HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API ServiceManager;
}

namespace KxVFS
{
	struct ServiceConfig
	{
		KxDynamicStringW BinaryPath;
		KxDynamicStringW LoadOrderGroup;
		KxDynamicStringW DisplayName;
		KxDynamicStringW Description;
		KxDynamicStringW StartName;
		ServiceType Type = ServiceType::None;
		ServiceStartMode StartMode = ServiceStartMode::None;
		ServiceErrorControl ErrorControl = ServiceErrorControl::Ignore;
		uint32_t TagID = 0;
	};
}

namespace KxVFS
{
	class KxVFS_API ServiceHandle: public HandleWrapper<ServiceHandle, SC_HANDLE, size_t, 0>
	{
		friend class TWrapper;
		
		protected:
			static void DoCloseHandle(THandle handle);

		public:
			ServiceHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}

		public:
			bool Create(ServiceManager& serviceManger,
						ServiceStartMode startType,
						KxDynamicStringRefW binaryPath,
						KxDynamicStringRefW serviceName,
						KxDynamicStringRefW displayName = {},
						KxDynamicStringRefW description = {}
			);
			bool Open(ServiceManager& serviceManger,
					  KxDynamicStringRefW serviceName,
					  ServiceAccess serviceAccess,
					  AccessRights otherRights = AccessRights::None
			);

			std::optional<ServiceConfig> GetConfig() const;
			bool SetConfig(ServiceManager& serviceManger,
							 KxDynamicStringRefW binaryPath,
							 ServiceType type,
							 ServiceStartMode startMode,
							 ServiceErrorControl errorControl
			);
			
			KxDynamicStringW GetDescription() const;
			bool SetDescription(KxDynamicStringRefW description);
			ServiceStatus GetStatus() const;

			bool Start();
			bool Stop();
			bool Delete();
	};
}
