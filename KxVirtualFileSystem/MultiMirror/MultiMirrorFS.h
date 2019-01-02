/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "AbstractFS.h"
#include "IRequestDispatcher.h"
#include "IEnumerationDispatcher.h"
#include "Convergence/ConvergenceFS.h"
#include "Utility.h"

namespace KxVFS
{
	class Service;

	class KxVFS_API MultiMirrorFS: public ConvergenceFS
	{
		private:
			using RedirectionPathsListT = std::vector<std::wstring>;

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

			bool CanDeleteInVirtualFolder() const = delete;
			bool SetCanDeleteInVirtualFolder(bool value) = delete;

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
			MultiMirrorFS(Service* vfsService, const WCHAR* mountPoint, const WCHAR* source, ULONG falgs = DefFlags, ULONG requestTimeout = DefRequestTimeout);
			virtual ~MultiMirrorFS();

		protected:
			DWORD OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, KxVFS::Utility::StringSearcherHash& hashStore);
			virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
	};
}
