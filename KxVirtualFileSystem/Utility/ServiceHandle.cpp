#include "stdafx.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ServiceHandle.h"
#include "ServiceManager.h"

namespace KxVFS
{
	void ServiceHandle::DoCloseHandle(THandle handle) noexcept
	{
		::CloseServiceHandle(handle);
	}

	bool ServiceHandle::Create(ServiceManager& serviceManger,
							   ServiceStartMode startType,
							   DynamicStringRefW binaryPath,
							   DynamicStringRefW serviceName,
							   DynamicStringRefW displayName,
							   DynamicStringRefW description
	) noexcept
	{
		Assign(::CreateServiceW
		(
			serviceManger,
			serviceName.data(),
			displayName.data(),
			SERVICE_ALL_ACCESS,
			SERVICE_FILE_SYSTEM_DRIVER,
			ToInt(startType),
			SERVICE_ERROR_IGNORE,
			binaryPath.data(),
			nullptr, nullptr, nullptr, nullptr, nullptr
		));
		if (!IsNull())
		{
			if (!description.empty())
			{
				SetDescription(description);
			}
			return true;
		}
		return false;
	}
	bool ServiceHandle::Open(ServiceManager& serviceManger,
							 DynamicStringRefW serviceName,
							 FlagSet<ServiceAccess> serviceAccess,
							 FlagSet<AccessRights> otherRights
	) noexcept
	{
		return Assign(::OpenServiceW(serviceManger, serviceName.data(), serviceAccess.ToInt()|otherRights.ToInt()));
	}
	
	std::optional<ServiceConfig> ServiceHandle::GetConfig() const
	{
		DWORD reqSize = 0;
		if (!::QueryServiceConfigW(m_Handle, nullptr, 0, &reqSize) && reqSize != 0)
		{
			std::vector<uint8_t> buffer(reqSize, 0);
			QUERY_SERVICE_CONFIGW& serviceConfig = *reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
			if (::QueryServiceConfigW(m_Handle, &serviceConfig, reqSize, &reqSize))
			{
				ServiceConfig config;

				config.BinaryPath = serviceConfig.lpBinaryPathName;
				if (config.BinaryPath.length() > 4)
				{
					config.BinaryPath.erase(0, 4); // Remove '\??\' from the beginning
				}

				config.DisplayName = serviceConfig.lpDisplayName;
				config.StartName = serviceConfig.lpServiceStartName;
				config.LoadOrderGroup = serviceConfig.lpLoadOrderGroup;
				config.Type = FromInt<ServiceType>(serviceConfig.dwServiceType);
				config.StartMode = FromInt<ServiceStartMode>(serviceConfig.dwStartType);
				config.ErrorControl = FromInt<ServiceErrorControl>(serviceConfig.dwErrorControl);
				config.TagID = serviceConfig.dwTagId;

				return config;
			}
		}
		return std::nullopt;
	}
	bool ServiceHandle::SetConfig(ServiceManager& serviceManger,
									DynamicStringRefW binaryPath,
									ServiceType type,
									ServiceStartMode startMode,
									ServiceErrorControl errorControl
	) noexcept
	{
		const bool success = ChangeServiceConfigW
		(
			m_Handle,
			ToInt(type),
			ToInt(startMode),
			ToInt(errorControl),
			binaryPath.data(),
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
		);
		return success;
	}

	DynamicStringW ServiceHandle::GetDescription() const
	{
		DWORD reqSize = 0;
		if (!::QueryServiceConfig2W(m_Handle, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &reqSize) && reqSize != 0)
		{
			std::vector<uint8_t> buffer(reqSize, 0);
			if (::QueryServiceConfig2W(m_Handle, SERVICE_CONFIG_DESCRIPTION, buffer.data(), reqSize, &reqSize))
			{
				SERVICE_DESCRIPTION& desc = *reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.data());
				return desc.lpDescription;
			}
		}
		return {};
	}
	bool ServiceHandle::SetDescription(DynamicStringRefW description) noexcept
	{
		SERVICE_DESCRIPTION desc = {};
		desc.lpDescription = const_cast<LPWSTR>(description.data());
		return ::ChangeServiceConfig2W(m_Handle, SERVICE_CONFIG_DESCRIPTION, &desc);
	}
	ServiceStatus ServiceHandle::GetStatus() const noexcept
	{
		DWORD reqSize = 0;
		SERVICE_STATUS_PROCESS status = {0};
		if (::QueryServiceStatusEx(m_Handle, SC_STATUS_PROCESS_INFO, (BYTE*)&status, sizeof(status), &reqSize))
		{
			return FromInt<ServiceStatus>(status.dwCurrentState);
		}
		return ServiceStatus::Unknown;
	}

	bool ServiceHandle::Start() noexcept
	{
		return ::StartServiceW(m_Handle, 0, nullptr);
	}
	bool ServiceHandle::Stop() noexcept
	{
		SERVICE_STATUS status = {0};
		return ::ControlService(m_Handle, SERVICE_STOP, &status);
	}
	bool ServiceHandle::Delete() noexcept
	{
		return ::DeleteService(m_Handle);
	}
}
