/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "IFileSystem.h"
#include "Utility.h"
#include "Misc/IncludeDokan.h"
#include "Common/FSError.h"
#include "Common/FSFlags.h"
#include "Common/FileContext.h"
#include "Common/AsyncIOContext.h"
#include "Common/FileNode.h"
#include "Common/IOManager.h"
#include "Common/FileContextManager.h"
#include "Common/IRequestDispatcher.h"

namespace KxVFS
{
	class KxVFS_API BasicFileSystem: public IFileSystem
	{
		protected:
			static bool OnFileFound(EvtFindFiles& eventInfo, const WIN32_FIND_DATAW& findData)
			{
				return eventInfo.FillFindData(&eventInfo, const_cast<WIN32_FIND_DATAW*>(&findData)) == 0;
			}

		private:
			FileSystemService& m_Service;
			Dokany2::DOKAN_OPTIONS m_Options = {0};
			Dokany2::DOKAN_OPERATIONS m_Operations = {0};
			Dokany2::DOKAN_HANDLE m_Handle = nullptr;

			IOManager m_IOManager;
			FileContextManager m_FileContextManager;

			KxDynamicStringW m_MountPoint;
			CriticalSection m_UnmountCS;
			FSFlags m_Flags = FSFlags::None;
			bool m_IsMounted = false;
			bool m_IsDestructing = false;

		private:
			FSError DoMount();
			bool DoUnMount();

		public:
			BasicFileSystem(FileSystemService& service, KxDynamicStringRefW mountPoint, FSFlags flags = FSFlags::None);
			virtual ~BasicFileSystem();

		public:
			bool IsMounted() const override
			{
				return m_IsMounted;
			}
			FSError Mount() override;
			bool UnMount() override;

			KxDynamicStringW GetVolumeLabel() const;
			KxDynamicStringW GetVolumeFileSystem() const;
			uint32_t GetVolumeSerialNumber() const;

			FileSystemService& GetService() override
			{
				return m_Service;
			}
			FileContextManager& GetFileContextManager() override
			{
				return m_FileContextManager;
			}
			IOManager& GetIOManager() override
			{
				return m_IOManager;
			}

			virtual KxDynamicStringW GetMountPoint() const override
			{
				return m_MountPoint;
			}
			virtual void SetMountPoint(KxDynamicStringRefW mountPoint) override
			{
				m_MountPoint = mountPoint;
			}
			
			FSFlags GetFlags() const
			{
				return m_Flags;
			}
			void SetFlags(FSFlags flags)
			{
				m_Flags = flags;
			}

		private:
			NTSTATUS OnMountInternal(EvtMounted& eventInfo);
			NTSTATUS OnUnMountInternal(EvtUnMounted& eventInfo);

		private:
			static BasicFileSystem* GetFromContext(Dokany2::DOKAN_OPTIONS* dokanOptions)
			{
				return static_cast<BasicFileSystem*>(reinterpret_cast<IFileSystem*>(dokanOptions->GlobalContext));
			}
			template<class T> static IFileSystem* GetFromContext(const T* eventInfo)
			{
				return GetFromContext(eventInfo->DokanFileInfo->DokanOptions);
			}

			// Dokan callbacks
			static void DOKAN_CALLBACK Dokan_Mount(EvtMounted* eventInfo);
			static void DOKAN_CALLBACK Dokan_Unmount(EvtUnMounted* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeFreeSpace(EvtGetVolumeFreeSpace* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeInfo(EvtGetVolumeInfo* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetVolumeAttributes(EvtGetVolumeAttributes* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_CreateFile(EvtCreateFile* eventInfo);
			static void DOKAN_CALLBACK Dokan_CloseFile(EvtCloseFile* eventInfo);
			static void DOKAN_CALLBACK Dokan_CleanUp(EvtCleanUp* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_MoveFile(EvtMoveFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_CanDeleteFile(EvtCanDeleteFile* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_LockFile(EvtLockFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_UnlockFile(EvtUnlockFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetFileSecurity(EvtGetFileSecurity* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetFileSecurity(EvtSetFileSecurity* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_ReadFile(EvtReadFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_WriteFile(EvtWriteFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_FlushFileBuffers(EvtFlushFileBuffers* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetEndOfFile(EvtSetEndOfFile* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetAllocationSize(EvtSetAllocationSize* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_GetFileInfo(EvtGetFileInfo* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_SetBasicFileInfo(EvtSetBasicFileInfo* eventInfo);

			static NTSTATUS DOKAN_CALLBACK Dokan_FindFiles(EvtFindFiles* eventInfo);
			static NTSTATUS DOKAN_CALLBACK Dokan_FindStreams(EvtFindStreams* eventInfo);
	};
}
