#include "stdafx.h"
#include "KxVFS/Misc/IncludeWindows.h"
#include "KxVFS/Logger/ILogger.h"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/Utility.h"
#include "FileContextManager.h"
#include "IOManager.h"

namespace KxVFS
{
	bool FileContextManager::Init()
	{
		if (!m_IsInitialized)
		{
			m_FileContextPool.reserve(128);
			m_IsInitialized = true;
			return true;
		}
		return false;
	}
	void FileContextManager::Cleanup() noexcept
	{
		if (m_IsInitialized)
		{
			if (CriticalSectionLocker lock(m_FileContextPoolCS); true)
			{
				for (FileContext* fileContext: m_FileContextPool)
				{
					DeleteContext(fileContext);
				}

				m_FileContextPool.clear();
				m_FileContextPool.shrink_to_fit();
				m_FileContextPoolMaxSize = 0;
			}
			m_IsInitialized = false;
		}
	}
	
	void FileContextManager::PushContext(FileContext& fileContext)
	{
		m_IOManager.OnPushFileContext(fileContext);
		if (CriticalSectionLocker lock(m_FileContextPoolCS); true)
		{
			m_FileContextPool.push_back(&fileContext);
			m_FileContextPoolMaxSize = std::max(m_FileContextPoolMaxSize, m_FileContextPool.size());
		}
	}
	void FileContextManager::DeleteContext(FileContext* fileContext) noexcept
	{
		if (fileContext)
		{
			m_IOManager.OnDeleteFileContext(*fileContext);
			delete fileContext;
		}
	}
	FileContext* FileContextManager::PopContext(FileHandle fileHandle) noexcept
	{
		if (!fileHandle || m_IsUnmounted)
		{
			return nullptr;
		}

		FileContext* fileContext = nullptr;
		if (CriticalSectionLocker lock(m_FileContextPoolCS); !m_FileContextPool.empty())
		{
			fileContext = m_FileContextPool.back();
			m_FileContextPool.pop_back();
		}

		if (!fileContext)
		{
			fileContext = new(std::nothrow) FileContext(m_FileSystem);
			if (!fileContext)
			{
				KxVFS_Log(LogLevel::Fatal, L"%1: Unable allocate memory for 'FileContext'", __FUNCTIONW__);
			}
		}
		fileContext->AssignHandle(std::move(fileHandle));
		fileContext->MarkOpen();
		fileContext->MarkActive();
		fileContext->GetEventInfo().Reset();

		if (!m_IOManager.OnPopFileContext(*fileContext))
		{
			fileContext = nullptr;
		}
		return fileContext;
	}

	FileContextManager::FileContextManager(IFileSystem& fileSystem) noexcept
		:m_FileSystem(fileSystem), m_IOManager(fileSystem.GetIOManager())
	{
	}
}
