#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "Win32Constants.h"
#include "HandleWrapper.h"

namespace KxVFS
{
	class KxVFS_API ProcessHandle: public HandleWrapper<ProcessHandle, HANDLE, size_t, 0>
	{
		friend class TWrapper;

		public:
			static ProcessHandle GetCurrent() noexcept
			{
				return ::GetCurrentProcess();
			}
			static std::vector<uint32_t> EnumActiveProcesses() noexcept;

		protected:
			static void DoCloseHandle(THandle handle) noexcept
			{
				::CloseHandle(handle);
			}

		public:
			ProcessHandle(THandle fileHandle = GetInvalidHandle()) noexcept
				:HandleWrapper(fileHandle)
			{
			}
			ProcessHandle(uint32_t pid, FlagSet<AccessRights> access, bool inheritHandle = false) noexcept
				:HandleWrapper(::OpenProcess(access.ToInt(), inheritHandle, pid))
			{
			}

		public:
			uint32_t GetID() const noexcept
			{
				return ::GetProcessId(m_Handle);
			}
			uint32_t GetExitCode() const noexcept
			{
				DWORD exitCode = std::numeric_limits<DWORD>::max();
				::GetExitCodeProcess(m_Handle, &exitCode);
				return exitCode;
			}
			bool IsActive() const noexcept
			{
				return GetExitCode() == STILL_ACTIVE;
			}
			DynamicStringW GetImagePath() const;

			FlagSet<PriorityClass> GetPriority() const noexcept
			{
				return FromInt<PriorityClass>(::GetPriorityClass(m_Handle));
			}
			bool SetPriority(FlagSet<PriorityClass> priority) noexcept
			{
				return ::SetPriorityClass(m_Handle, priority.ToInt());
			}

			ProcessHandle Duplicate(bool inheritHandle = false) const noexcept;
			ProcessHandle Duplicate(FlagSet<AccessRights> access, bool inheritHandle = false) const noexcept;
	};
}
