#include "stdafx.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Logger/ILogger.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "IOManager.h"
#include "FileContextManager.h"

namespace KxVFS
{
	bool IOManager::InitializeAsyncIO()
	{
		if (!m_IsInitialized)
		{
			if (!InitializePendingAsyncIO())
			{
				return false;
			}
			m_AsyncContextPool.reserve(128);

			m_IsInitialized = true;
			return true;
		}
		return false;
	}
	bool IOManager::InitializePendingAsyncIO()
	{
		m_ThreadPool = Dokany2::DokanGetThreadPool();
		if (!m_ThreadPool)
		{
			return false;
		}

		m_ThreadPoolCleanupGroup = ::CreateThreadpoolCleanupGroup();
		if (!m_ThreadPoolCleanupGroup)
		{
			return false;
		}

		::InitializeThreadpoolEnvironment(&m_ThreadPoolEnvironment);
		::SetThreadpoolCallbackPool(&m_ThreadPoolEnvironment, m_ThreadPool);
		::SetThreadpoolCallbackCleanupGroup(&m_ThreadPoolEnvironment, m_ThreadPoolCleanupGroup, nullptr);
		return true;
	}
	
	void IOManager::CleanupPendingAsyncIO()
	{
		if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
		{
			::CloseThreadpoolCleanupGroupMembers(m_ThreadPoolCleanupGroup, FALSE, nullptr);
			::CloseThreadpoolCleanupGroup(m_ThreadPoolCleanupGroup);
			m_ThreadPoolCleanupGroup = nullptr;

			::DestroyThreadpoolEnvironment(&m_ThreadPoolEnvironment);
		}
	}
	void IOManager::CleanupAsyncIO()
	{
		if (m_IsInitialized)
		{
			CleanupPendingAsyncIO();
			if (CriticalSectionLocker lock(m_AsyncContextPoolCS); true)
			{
				for (AsyncIOContext* asyncContext: m_AsyncContextPool)
				{
					DeleteContext(asyncContext);
				}

				m_AsyncContextPool.clear();
				m_AsyncContextPool.shrink_to_fit();
				m_AsyncContextPoolMaxSize = 0;
			}

			m_IsInitialized = false;
		}
	}
	
	void CALLBACK IOManager::AsyncCallback(PTP_CALLBACK_INSTANCE instance,
										   PVOID context,
										   PVOID overlapped,
										   ULONG resultIO,
										   ULONG_PTR bytesTransferred,
										   PTP_IO completionPort
	)
	{
		FileContext& fileContext = *reinterpret_cast<FileContext*>(context);
		AsyncIOContext& asyncContext = *reinterpret_cast<AsyncIOContext*>(overlapped);
		IFileSystem& fileSystemInstance = fileContext.GetFileSystem();

		switch (asyncContext.GetOperationType())
		{
			case AsyncIOContext::OperationType::Read:
			{
				EvtReadFile& readFileEvent = *asyncContext.GetOperationContext<EvtReadFile>();
				readFileEvent.NumberOfBytesRead = static_cast<DWORD>(bytesTransferred);
				fileSystemInstance.OnFileRead(readFileEvent, fileContext);

				Dokany2::DokanEndDispatchRead(&readFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
			case AsyncIOContext::OperationType::Write:
			{
				EvtWriteFile& writeFileEvent = *asyncContext.GetOperationContext<EvtWriteFile>();
				writeFileEvent.NumberOfBytesWritten = static_cast<DWORD>(bytesTransferred);
				fileSystemInstance.OnFileWritten(writeFileEvent, fileContext);

				Dokany2::DokanEndDispatchWrite(&writeFileEvent, Dokany2::DokanNtStatusFromWin32(resultIO));
				break;
			}
			default:
			{
				KxVFS_Log(LogLevel::Info, L"Unknown operation type in async IO callback: %1", asyncContext.GetOperationType());
				break;
			}
		};

		fileSystemInstance.GetIOManager().PushContext(asyncContext);
	}

	void IOManager::OnDeleteFileContext(FileContext& fileContext)
	{
		if (m_IsAsyncIOEnabled && fileContext.IsThreadpoolIOCreated())
		{
			if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup && fileContext.IsThreadpoolIOCreated())
			{
				fileContext.CloseThreadpoolIO();
			}
		}
	}
	void IOManager::OnPushFileContext(FileContext& fileContext)
	{
		if (m_IsAsyncIOEnabled && fileContext.IsThreadpoolIOCreated())
		{
			if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
			{
				fileContext.CloseThreadpoolIO();
			}
		}
	}
	bool IOManager::OnPopFileContext(FileContext& fileContext)
	{
		if (m_IsAsyncIOEnabled)
		{
			bool success = false;
			if (CriticalSectionLocker lock(m_ThreadPoolCS); m_ThreadPoolCleanupGroup)
			{
				success = fileContext.CreateThreadpoolIO(AsyncCallback, m_ThreadPoolEnvironment);
			}

			if (!success)
			{
				m_FileContextManager.PushContext(fileContext);
				return false;
			}
		}
		return true;
	}

	IOManager::IOManager(IFileSystem& fileSystem)
		:m_FileSystem(fileSystem), m_FileContextManager(fileSystem.GetFileContextManager())
	{
	}

	bool IOManager::Init()
	{
		if (m_IsAsyncIOEnabled)
		{
			return InitializeAsyncIO();
		}
		return false;
	}
	void IOManager::Cleanup()
	{
		if (m_IsAsyncIOEnabled)
		{
			CleanupAsyncIO();
		}
	}

	void IOManager::DeleteContext(AsyncIOContext* asyncContext)
	{
		delete asyncContext;
	}
	void IOManager::PushContext(AsyncIOContext& asyncContext)
	{
		if (CriticalSectionLocker lock(m_AsyncContextPoolCS); true)
		{
			m_AsyncContextPool.push_back(&asyncContext);
			m_AsyncContextPoolMaxSize = std::max(m_AsyncContextPoolMaxSize, m_AsyncContextPool.size());
		}
	}
	AsyncIOContext* IOManager::PopContext(FileContext& fileContext)
	{
		AsyncIOContext* asyncContext = nullptr;
		if (CriticalSectionLocker lock(m_AsyncContextPoolCS); !m_AsyncContextPool.empty())
		{
			asyncContext = m_AsyncContextPool.back();
			m_AsyncContextPool.pop_back();
		}

		if (!asyncContext)
		{
			asyncContext = new(std::nothrow) AsyncIOContext(fileContext);
			if (!asyncContext)
			{
				KxVFS_Log(LogLevel::Fatal, L"%1: Unable allocate memory for 'AsyncIOContext'", __FUNCTIONW__);
			}
		}
		else
		{
			asyncContext->AssignFileContext(fileContext);
		}
		return asyncContext;
	}

	NtStatus IOManager::ReadFileSync(FileHandle& fileHandle, EvtReadFile& eventInfo, FileContext* fileContext) const
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, fileHandle.GetPath());

		if (!fileHandle.Seek(eventInfo.Offset, FileSeekMode::Start))
		{
			return IFileSystem::GetNtStatusByWin32LastErrorCode();
		}
		if (fileHandle.Read(eventInfo.Buffer, eventInfo.NumberOfBytesToRead, eventInfo.NumberOfBytesRead))
		{
			if (fileContext)
			{
				m_FileSystem.OnFileRead(eventInfo, *fileContext);
			}
			return NtStatus::Success;
		}
		return IFileSystem::GetNtStatusByWin32LastErrorCode();
	}
	NtStatus IOManager::WriteFileSync(FileHandle& fileHandle, EvtWriteFile& eventInfo, FileContext* fileContext) const
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, fileHandle.GetPath());

		if (eventInfo.DokanFileInfo->WriteToEndOfFile)
		{
			if (!fileHandle.Seek(0, FileSeekMode::End))
			{
				return IFileSystem::GetNtStatusByWin32LastErrorCode();
			}
		}
		else
		{
			int64_t fileSize = 0;
			if (!fileHandle.GetFileSize(fileSize))
			{
				return IFileSystem::GetNtStatusByWin32LastErrorCode();
			}

			// Paging IO can not write after allocated file size
			if (eventInfo.DokanFileInfo->PagingIo)
			{
				if (eventInfo.Offset >= fileSize)
				{
					eventInfo.NumberOfBytesWritten = 0;
					return NtStatus::Success;
				}

				// WTF is happening here?
				if ((uint64_t)(eventInfo.Offset + eventInfo.NumberOfBytesToWrite) > (uint64_t)fileSize)
				{
					uint64_t bytes = fileSize - eventInfo.Offset;
					if (bytes >> 32)
					{
						eventInfo.NumberOfBytesToWrite = (DWORD)(bytes & 0xFFFFFFFFUL);
					}
					else
					{

						eventInfo.NumberOfBytesToWrite = (DWORD)bytes;
					}
				}
			}

			if (eventInfo.Offset > fileSize)
			{
				// In the mirror sample helperZeroFileData is not necessary. NTFS will zero a hole.
				// But if user's file system is different from NTFS (or other Windows
				// file systems) then  users will have to zero the hole themselves.

				// If only Dokany devs can explain more clearly what they are talking about
			}

			if (!fileHandle.Seek(eventInfo.Offset, FileSeekMode::Start))
			{
				return IFileSystem::GetNtStatusByWin32LastErrorCode();
			}
		}

		if (fileHandle.Write(eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, eventInfo.NumberOfBytesWritten))
		{
			if (fileContext)
			{
				m_FileSystem.OnFileWritten(eventInfo, *fileContext);
			}
			return NtStatus::Success;
		}
		return IFileSystem::GetNtStatusByWin32LastErrorCode();
	}

	NtStatus IOManager::ReadFileAsync(FileContext& fileContext, EvtReadFile& eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, fileContext.GetHandle().GetPath());

		AsyncIOContext* asyncContext = PopContext(fileContext);
		if (!asyncContext)
		{
			return NtStatus::MemoryNotAllocated;
		}
		asyncContext->SetOperationContext(eventInfo, eventInfo.Offset, AsyncIOContext::OperationType::Read);

		fileContext.StartThreadpoolIO();
		if (!fileContext.GetHandle().Read(eventInfo.Buffer, eventInfo.NumberOfBytesToRead, eventInfo.NumberOfBytesRead, &asyncContext->GetOverlapped()))
		{
			const DWORD errorCode = ::GetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				fileContext.CancelThreadpoolIO();
				return IFileSystem::GetNtStatusByWin32ErrorCode(errorCode);
			}
		}
		return NtStatus::Pending;
	}
	NtStatus IOManager::WriteFileAsync(FileContext& fileContext, EvtWriteFile& eventInfo)
	{
		KxVFS_Log(LogLevel::Info, L"%1: %2", __FUNCTIONW__, fileContext.GetHandle().GetPath());

		FileHandle& fileHandle = fileContext.GetHandle();

		int64_t fileSize = 0;
		if (!fileHandle.GetFileSize(fileSize))
		{
			return IFileSystem::GetNtStatusByWin32LastErrorCode();
		}

		// Paging IO, I need to read about it at some point. Until then, don't touch this code.
		if (eventInfo.DokanFileInfo->PagingIo)
		{
			if (eventInfo.Offset >= fileSize)
			{
				eventInfo.NumberOfBytesWritten = 0;
				return NtStatus::Success;
			}

			if ((uint64_t)(eventInfo.Offset + eventInfo.NumberOfBytesToWrite) > (uint64_t)fileSize)
			{
				uint64_t bytes = fileSize - eventInfo.Offset;
				if (bytes >> 32)
				{
					eventInfo.NumberOfBytesToWrite = static_cast<DWORD>(bytes & std::numeric_limits<DWORD>::max());
				}
				else
				{
					eventInfo.NumberOfBytesToWrite = static_cast<DWORD>(bytes);
				}
			}
		}

		AsyncIOContext* asyncContext = PopContext(fileContext);
		if (!asyncContext)
		{
			return NtStatus::MemoryNotAllocated;
		}
		asyncContext->SetOperationContext(eventInfo, eventInfo.Offset, AsyncIOContext::OperationType::Write);

		fileContext.StartThreadpoolIO();
		if (!fileHandle.Write(eventInfo.Buffer, eventInfo.NumberOfBytesToWrite, eventInfo.NumberOfBytesWritten, &asyncContext->GetOverlapped()))
		{
			const DWORD errorCode = ::GetLastError();
			if (errorCode != ERROR_IO_PENDING)
			{
				fileContext.CancelThreadpoolIO();
				return IFileSystem::GetNtStatusByWin32ErrorCode(errorCode);
			}
		}
		return NtStatus::Pending;
	}
}
