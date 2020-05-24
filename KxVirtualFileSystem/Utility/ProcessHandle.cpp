#include "stdafx.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "ProcessHandle.h"
#include <Psapi.h>

namespace
{
	HANDLE DuplicateProcessHandle(HANDLE handle, uint32_t access, bool inherit, uint32_t options)
	{
		const HANDLE current = ::GetCurrentProcess();

		HANDLE duplicate = nullptr;
		::DuplicateHandle(current, handle, current, &duplicate, access, inherit, options);
		return duplicate;
	}
}

namespace KxVFS
{
	std::vector<uint32_t> ProcessHandle::EnumActiveProcesses()
	{
		DWORD processes[1024] = {};
		DWORD retSize = 0;

		if (::EnumProcesses(processes, static_cast<DWORD>(std::size(processes) * sizeof(DWORD)), &retSize))
		{
			const DWORD processesCount = retSize / sizeof(DWORD);

			std::vector<uint32_t> processIDs;
			processIDs.reserve(processesCount);
			for (size_t i = 0; i < processesCount; i++)
			{
				processIDs.emplace_back(processes[i]);
			}

			return processIDs;
		}
		return {};
	}
	DynamicStringW ProcessHandle::GetImagePath() const
	{
		DynamicStringW path;

		// Try using short static storage
		DWORD length = static_cast<DWORD>(path.max_size_static()) - 1;
		path.resize(length);

		if (!::QueryFullProcessImageNameW(m_Handle, 0, path.data(), &length))
		{
			// If too small allocate maximum possible path name length and call again
			length = std::numeric_limits<int16_t>::max();
			path.resize(length);

			if (!::QueryFullProcessImageNameW(m_Handle, 0, path.data(), &length))
			{
				length = 0;
			}
		}

		// Here 'length' contains valid final path length
		path.resize(length);
		return path;
	}

	ProcessHandle ProcessHandle::Duplicate(bool inheritHandle) const
	{
		return DuplicateProcessHandle(m_Handle, 0, inheritHandle, 0);
	}
	ProcessHandle ProcessHandle::Duplicate(AccessRights access, bool inheritHandle) const
	{
		return DuplicateProcessHandle(m_Handle, ToInt(access), inheritHandle, 0);
	}
}
