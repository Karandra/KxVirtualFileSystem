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
	struct ServiceConfig final
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
			static void DoCloseHandle(THandle handle) noexcept;

		public:
			ServiceHandle(THandle fileHandle = GetInvalidHandle()) noexcept
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
			) noexcept;
			bool Open(ServiceManager& serviceManger,
					  DynamicStringRefW serviceName,
					  FlagSet<ServiceAccess> serviceAccess,
					  FlagSet<AccessRights> otherRights = {}
			) noexcept;

			std::optional<ServiceConfig> GetConfig() const;
			bool SetConfig(ServiceManager& serviceManger,
						   DynamicStringRefW binaryPath,
						   ServiceType type,
						   ServiceStartMode startMode,
						   ServiceErrorControl errorControl
			) noexcept;
			
			DynamicStringW GetDescription() const;
			bool SetDescription(DynamicStringRefW description) noexcept;
			ServiceStatus GetStatus() const noexcept;

			bool Start() noexcept;
			bool Stop() noexcept;
			bool Delete() noexcept;
	};
}
