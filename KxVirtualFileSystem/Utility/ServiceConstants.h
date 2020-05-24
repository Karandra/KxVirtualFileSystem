#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"

namespace KxVFS
{
	enum class ServiceStartMode: uint32_t
	{
		None = 0,

		Auto = SERVICE_AUTO_START,
		Boot = SERVICE_BOOT_START,
		System = SERVICE_SYSTEM_START,
		OnDemand = SERVICE_DEMAND_START,
		Disabled = SERVICE_DISABLED,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceStartMode);

	enum class ServiceStatus: uint32_t
	{
		Unknown = 0,

		Stopped = SERVICE_STOPPED,
		Paused = SERVICE_PAUSED,
		Running = SERVICE_RUNNING,

		PendingStart = SERVICE_START_PENDING,
		PendingStop = SERVICE_STOP_PENDING,
		PendingPause = SERVICE_PAUSE_PENDING,
		PendingContinue = SERVICE_CONTINUE_PENDING,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceStatus);

	enum class ServiceAccess: uint32_t
	{
		None = 0,

		QueryConfig = SERVICE_QUERY_CONFIG,
		QueryStatus = SERVICE_QUERY_STATUS,
		ChangeConfig = SERVICE_CHANGE_CONFIG,
		EnumerateDependents = SERVICE_ENUMERATE_DEPENDENTS,

		Start = SERVICE_START,
		Stop = SERVICE_STOP,
		Delete = DELETE,
		Interrogate = SERVICE_INTERROGATE,
		PauseContinue = SERVICE_PAUSE_CONTINUE,
		UserDefinedControl = SERVICE_USER_DEFINED_CONTROL,

		All = SERVICE_ALL_ACCESS,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceAccess);

	enum class ServiceManagerAccess: uint32_t
	{
		None = 0,

		All = SC_MANAGER_ALL_ACCESS,
		Lock = SC_MANAGER_LOCK,
		Connect = SC_MANAGER_CONNECT,
		Enumerate = SC_MANAGER_ENUMERATE_SERVICE,
		CreateService = SC_MANAGER_CREATE_SERVICE,
		QueryLockStatus = SC_MANAGER_QUERY_LOCK_STATUS,
		ChangeBootConfig = SC_MANAGER_MODIFY_BOOT_CONFIG,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceManagerAccess);

	enum class ServiceType: uint32_t
	{
		None = 0,

		FileSystemDriver = SERVICE_FILE_SYSTEM_DRIVER,
		KernelDriver = SERVICE_KERNEL_DRIVER,
		Win32OwnProcess = SERVICE_WIN32_OWN_PROCESS,
		Win32SharedProcess = SERVICE_WIN32_SHARE_PROCESS,
		InteractiveProcess = SERVICE_INTERACTIVE_PROCESS,
	};
	KxVFS_AllowEnumBitwiseOp(ServiceType);

	enum class ServiceErrorControl: uint32_t
	{
		Ignore = SERVICE_ERROR_IGNORE,
		Normal = SERVICE_ERROR_NORMAL,
		Severe = SERVICE_ERROR_SEVERE,
		Critical = SERVICE_ERROR_CRITICAL,
	};
	KxVFS_AllowEnumCastOp(ServiceErrorControl);
}
