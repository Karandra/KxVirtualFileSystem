/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/IRequestDispatcher.h"
#include "KxVirtualFileSystem/Utility.h"
#include "MirrorStructs.h"

namespace KxVFS
{
	class Service;

	class KxVFS_API MirrorFS: public AbstractFS
	{
		private:
			KxDynamicStringW m_Source;

		private:
			volatile LONG m_IsUnmounted = 0;
			bool m_IsUnsingAsyncIO = true;
			bool m_EnableSecurityFunctions = false;
			bool m_ShouldImpersonateCallerUser = false;

			Dokany2::DOKAN_VECTOR m_FileHandlePool = {0};
			CriticalSection m_FileHandlePoolCS;

			// Async IO
			TP_CALLBACK_ENVIRON m_ThreadPoolEnvironment = {0};
			PTP_CLEANUP_GROUP m_ThreadPoolCleanupGroup = nullptr;
			PTP_POOL m_ThreadPool = nullptr;
			CriticalSection m_ThreadPoolCS;
			Dokany2::DOKAN_VECTOR m_OverlappedPool = {0};
			CriticalSection m_OverlappedPoolCS;

		protected:
			// Security section
			DWORD GetParentSecurity(KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR* parentSecurity) const;
			DWORD CreateNewSecurity(EvtCreateFile& eventInfo, KxDynamicStringRefW filePath, PSECURITY_DESCRIPTOR requestedSecurity, PSECURITY_DESCRIPTOR* newSecurity) const;
			Utility::SecurityObject CreateSecurityIfNeeded(EvtCreateFile& eventInfo, SECURITY_ATTRIBUTES& securityAttributes, KxDynamicStringRefW targetPath, DWORD creationDisposition);
			
			void OpenWithSecurityAccess(ACCESS_MASK& desiredAccess, bool isWriteRequest) const;
			void OpenWithSecurityAccessIfNeeded(ACCESS_MASK& desiredAccess, bool isWriteRequest) const
			{
				if (m_EnableSecurityFunctions)
				{
					OpenWithSecurityAccess(desiredAccess, isWriteRequest);
				}
			}

			// ImpersonateCallerUser section
			TokenHandle ImpersonateCallerUser(EvtCreateFile& eventInfo) const;
			bool ImpersonateLoggedOnUser(TokenHandle& userTokenHandle) const;
			void CleanupImpersonateCallerUser(TokenHandle& userTokenHandle) const
			{
				// Something which is not 'STATUS_SUCCESS'.
				CleanupImpersonateCallerUser(userTokenHandle, STATUS_NOT_SUPPORTED);
			}
			void CleanupImpersonateCallerUser(TokenHandle& userTokenHandle, NTSTATUS status) const;

			TokenHandle ImpersonateCallerUserIfNeeded(EvtCreateFile& eventInfo) const
			{
				if (m_ShouldImpersonateCallerUser)
				{
					return ImpersonateCallerUser(eventInfo);
				}
				return TokenHandle();
			}
			bool ImpersonateLoggedOnUserIfNeeded(TokenHandle& userTokenHandle) const
			{
				if (m_ShouldImpersonateCallerUser)
				{
					return ImpersonateLoggedOnUser(userTokenHandle);
				}
				return false;
			}
			void CleanupImpersonateCallerUserIfNeeded(TokenHandle& userTokenHandle) const
			{
				if (m_ShouldImpersonateCallerUser)
				{
					return CleanupImpersonateCallerUser(userTokenHandle);
				}
			}
			void CleanupImpersonateCallerUserIfNeeded(TokenHandle& userTokenHandle, NTSTATUS status) const
			{
				if (m_ShouldImpersonateCallerUser)
				{
					CleanupImpersonateCallerUser(userTokenHandle, status);
				}
			}

			// Sync Read/Write section
			NTSTATUS ReadFileSync(EvtReadFile& eventInfo, HANDLE fileHandle) const;
			NTSTATUS WriteFileSync(EvtWriteFile& eventInfo, HANDLE fileHandle, UINT64 fileSize) const;
			
			// On delete
			bool CheckDeleteOnClose(Dokany2::PDOKAN_FILE_INFO fileInfo, KxDynamicStringRefW filePath) const;
			NTSTATUS CanDeleteDirectory(KxDynamicStringRefW filePath) const;

			// IRequestDispatcher
		public:
			void DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) override;

		public:
			MirrorFS(Service& service, KxDynamicStringRefW mountPoint, KxDynamicStringRefW source, uint32_t flags = DefFlags);
			virtual ~MirrorFS();

		public:
			FSError Mount() override;

		public:
			KxDynamicStringRefW GetSource() const;
			bool SetSource(KxDynamicStringRefW source);

			bool IsUnsingAsyncIO() const;
			bool UseAsyncIO(bool value);

			bool IsSecurityFunctionsEnabled() const;
			bool EnableSecurityFunctions(bool value);

			bool ShouldImpersonateCallerUser() const;
			bool SetImpersonateCallerUser(bool value);

		protected:
			// Events for derived classes
			virtual void OnFileClosed(EvtCloseFile& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnFileCleanedUp(EvtCleanUp& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnDirectoryDeleted(EvtCanDeleteFile& eventInfo, const KxDynamicStringW& targetPath) { }
			
			virtual void OnFileRead(EvtReadFile& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnFileWritten(EvtWriteFile& eventInfo, const KxDynamicStringW& targetPath) { }

			virtual void OnFileBuffersFlushed(EvtFlushFileBuffers& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnAllocationSizeSet(EvtSetAllocationSize& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnEndOfFileSet(EvtSetEndOfFile& eventInfo, const KxDynamicStringW& targetPath) { }
			virtual void OnBasicFileInfoSet(EvtSetBasicFileInfo& eventInfo, const KxDynamicStringW& targetPath) { }

		protected:
			template<class TEventInfo> Mirror::FileContext* GetFileContext(TEventInfo& eventInfo) const
			{
				return reinterpret_cast<Mirror::FileContext*>(eventInfo.DokanFileInfo->Context);
			}
			template<class TEventInfo> void SaveFileContext(TEventInfo& eventInfo, Mirror::FileContext* fileContext) const
			{
				eventInfo.DokanFileInfo->Context = reinterpret_cast<ULONG64>(fileContext);
			}

			NTSTATUS OnMount(EvtMounted& eventInfo) override;
			NTSTATUS OnUnMount(EvtUnMounted& eventInfo) override;

			NTSTATUS OnGetVolumeFreeSpace(EvtGetVolumeFreeSpace& eventInfo) override;
			NTSTATUS OnGetVolumeInfo(EvtGetVolumeInfo& eventInfo) override;
			NTSTATUS OnGetVolumeAttributes(EvtGetVolumeAttributes& eventInfo) override;

			NTSTATUS OnCreateFile(EvtCreateFile& eventInfo) override;
			NTSTATUS OnCloseFile(EvtCloseFile& eventInfo) override;
			NTSTATUS OnCleanUp(EvtCleanUp& eventInfo) override;
			NTSTATUS OnMoveFile(EvtMoveFile& eventInfo) override;
			NTSTATUS OnCanDeleteFile(EvtCanDeleteFile& eventInfo) override;

			NTSTATUS OnLockFile(EvtLockFile& eventInfo) override;
			NTSTATUS OnUnlockFile(EvtUnlockFile& eventInfo) override;

			NTSTATUS OnGetFileSecurity(EvtGetFileSecurity& eventInfo) override;
			NTSTATUS OnSetFileSecurity(EvtSetFileSecurity& eventInfo) override;

			NTSTATUS OnReadFile(EvtReadFile& eventInfo) override;
			NTSTATUS OnWriteFile(EvtWriteFile& eventInfo) override;
			
			NTSTATUS OnFlushFileBuffers(EvtFlushFileBuffers& eventInfo) override;
			NTSTATUS OnSetEndOfFile(EvtSetEndOfFile& eventInfo) override;
			NTSTATUS OnSetAllocationSize(EvtSetAllocationSize& eventInfo) override;
			NTSTATUS OnGetFileInfo(EvtGetFileInfo& eventInfo) override;
			NTSTATUS OnSetBasicFileInfo(EvtSetBasicFileInfo& eventInfo) override;

			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
			NTSTATUS OnFindStreams(EvtFindStreams& eventInfo) override;

			// File context section
		protected:
			void FreeMirrorFileHandle(Mirror::FileContext* fileContext);
			void PushMirrorFileHandle(Mirror::FileContext* fileContext);
			Mirror::FileContext* PopMirrorFileHandle(HANDLE fileHandle);
			void GetMirrorFileHandleState(Mirror::FileContext* fileContext, bool* isCleanedUp, bool* isClosed) const;

			bool InitializeMirrorFileHandles();
			void CleanupMirrorFileHandles();

			// Async IO section
		protected:
			void FreeMirrorOverlapped(Mirror::OverlappedContext* overlappedContext) const;
			void PushMirrorOverlapped(Mirror::OverlappedContext* overlappedContext);
			Mirror::OverlappedContext* PopMirrorOverlapped();

			bool InitializeAsyncIO();
			void CleanupPendingAsyncIO();
			void CleanupAsyncIO();
			static void CALLBACK MirrorIoCallback(PTP_CALLBACK_INSTANCE instance,
												  PVOID context,
												  PVOID overlapped,
												  ULONG resultIO,
												  ULONG_PTR numberOfBytesTransferred,
												  PTP_IO portIO
			);
	};
}
