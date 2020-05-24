#pragma once
#include "KxVirtualFileSystem/Common.hpp"
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
		DynamicStringW BinaryPath;
		DynamicStringW LoadOrderGroup;
		DynamicStringW DisplayName;
		DynamicStringW Description;
		DynamicStringW StartName;
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
						DynamicStringRefW binaryPath,
						DynamicStringRefW serviceName,
						DynamicStringRefW displayName = {},
						DynamicStringRefW description = {}
			);
			bool Open(ServiceManager& serviceManger,
					  DynamicStringRefW serviceName,
					  ServiceAccess serviceAccess,
					  AccessRights otherRights = AccessRights::None
			);

			std::optional<ServiceConfig> GetConfig() const;
			bool SetConfig(ServiceManager& serviceManger,
							 DynamicStringRefW binaryPath,
							 ServiceType type,
							 ServiceStartMode startMode,
							 ServiceErrorControl errorControl
			);
			
			DynamicStringW GetDescription() const;
			bool SetDescription(DynamicStringRefW description);
			ServiceStatus GetStatus() const;

			bool Start();
			bool Stop();
			bool Delete();
	};
}
