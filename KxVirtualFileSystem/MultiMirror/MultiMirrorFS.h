/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/IRequestDispatcher.h"
#include "KxVirtualFileSystem/IEnumerationDispatcher.h"
#include "KxVirtualFileSystem/Convergence/ConvergenceFS.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class Service;

	class KxVFS_API MultiMirrorFS: public ConvergenceFS
	{
		private:
			bool UpdateDispatcherIndexUnlocked(const KxDynamicStringW& requestedPath, const KxDynamicStringW& targetPath) = delete;
			void UpdateDispatcherIndexUnlocked(const KxDynamicStringW& requestedPath) = delete;
			bool UpdateDispatcherIndex(const KxDynamicStringW& requestedPath, const KxDynamicStringW& targetPath) = delete;
			void UpdateDispatcherIndex(const KxDynamicStringW& requestedPath) = delete;
			KxDynamicStringW TryDispatchRequest(const KxDynamicStringW& requestedPath) const = delete;

			// Non-existent INI-files
			bool IsINIFile(KxDynamicStringRefW requestedPath) const = delete;
			bool IsINIFileNonExistent(KxDynamicStringRefW requestedPath) const = delete;
			void AddINIFile(KxDynamicStringRefW requestedPath) = delete;

			bool IsDeleteInVirtualFoldersAllowed() const = delete;
			bool AllowDeleteInVirtualFolder(bool value) = delete;

			void BuildDispatcherIndex() = delete;
			void SetDispatcherIndex(const ExternalDispatcherIndexT& index) = delete;
			template<class T> void SetDispatcherIndexT(const T& index) = delete;
			template<class T> void SetDispatcherIndex(const T& index) = delete;

			KxDynamicStringRefW GetWriteTarget() const = delete;
			bool SetWriteTarget(KxDynamicStringRefW writeTarget) = delete;

		protected:
			// IRequestDispatcher
			virtual void ResolveLocation(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) override;

		public:
			MultiMirrorFS(Service* vfsService, KxDynamicStringRefW mountPoint, KxDynamicStringRefW source, uint32_t flags = DefFlags);
			virtual ~MultiMirrorFS();

		protected:
			DWORD OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, KxVFS::Utility::StringSearcherHash& hashStore);
			virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
	};
}
