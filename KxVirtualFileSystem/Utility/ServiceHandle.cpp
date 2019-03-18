/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ServiceHandle.h"
#include "ServiceManager.h"

namespace KxVFS
{
	void ServiceHandle::DoCloseHandle(THandle handle)
	{
		::CloseServiceHandle(handle);
	}

	bool ServiceHandle::Create(ServiceManager& serviceManger,
							   ServiceStartType startType,
							   KxDynamicStringRefW binaryPath,
							   KxDynamicStringRefW serviceName,
							   KxDynamicStringRefW displayName,
							   KxDynamicStringRefW description
	)
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
		if (IsValid())
		{
			if (!description.empty())
			{
				SetDescription(description);
			}
			return true;
		}
		return false;
	}
	bool ServiceHandle::Open(ServiceManager& serviceManger, KxDynamicStringRefW serviceName, ServiceAccess serviceAccess, AccessRights otherRights)
	{
		Assign(::OpenServiceW(serviceManger, serviceName.data(), ToInt(serviceAccess)|ToInt(otherRights)));
		return IsValid();
	}
	bool ServiceHandle::Reconfigure(ServiceManager& serviceManger, ServiceStartType startType, KxDynamicStringRefW binaryPath)
	{
		const bool success = ChangeServiceConfigW
		(
			m_Handle,
			SERVICE_FILE_SYSTEM_DRIVER,
			ToInt(startType),
			SERVICE_ERROR_IGNORE,
			binaryPath.data(),
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
		);
		return success;
	}
	
	bool ServiceHandle::SetDescription(KxDynamicStringRefW description)
	{
		SERVICE_DESCRIPTION desc = {0};
		desc.lpDescription = const_cast<LPWSTR>(description.data());
		return ::ChangeServiceConfig2W(m_Handle, SERVICE_CONFIG_DESCRIPTION, &desc);
	}
	ServiceStatus ServiceHandle::GetStatus() const
	{
		DWORD reqSize = 0;
		SERVICE_STATUS_PROCESS status = {0};
		if (::QueryServiceStatusEx(m_Handle, SC_STATUS_PROCESS_INFO, (BYTE*)&status, sizeof(status), &reqSize))
		{
			return FromInt<ServiceStatus>(status.dwCurrentState);
		}
		return ServiceStatus::Unknown;
	}

	bool ServiceHandle::Start()
	{
		return ::StartServiceW(m_Handle, 0, nullptr);
	}
	bool ServiceHandle::Stop()
	{
		SERVICE_STATUS status = {0};
		return ::ControlService(m_Handle, SERVICE_STOP, &status);
	}
	bool ServiceHandle::Delete()
	{
		return ::DeleteService(m_Handle);
	}
}
