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
			static ProcessHandle GetCurrent()
			{
				return ::GetCurrentProcess();
			}
			static std::vector<uint32_t> EnumActiveProcesses();

		protected:
			static void DoCloseHandle(THandle handle)
			{
				::CloseHandle(handle);
			}

		public:
			ProcessHandle(THandle fileHandle = GetInvalidHandle())
				:HandleWrapper(fileHandle)
			{
			}
			ProcessHandle(uint32_t pid, AccessRights access, bool inheritHandle = false)
				:HandleWrapper(::OpenProcess(ToInt(access), inheritHandle, pid))
			{
			}

		public:
			uint32_t GetID() const
			{
				return ::GetProcessId(m_Handle);
			}
			uint32_t GetExitCode() const
			{
				DWORD exitCode = std::numeric_limits<DWORD>::max();
				::GetExitCodeProcess(m_Handle, &exitCode);
				return exitCode;
			}
			bool IsActive() const
			{
				return GetExitCode() == STILL_ACTIVE;
			}
			DynamicStringW GetImagePath() const;

			PriorityClass GetPriority() const
			{
				return FromInt<PriorityClass>(::GetPriorityClass(m_Handle));
			}
			bool SetPriority(PriorityClass priority)
			{
				return ::SetPriorityClass(m_Handle, ToInt(priority));
			}

			ProcessHandle Duplicate(bool inheritHandle = false) const;
			ProcessHandle Duplicate(AccessRights access, bool inheritHandle = false) const;
	};
}
