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
			bool UpdateDispatcherIndexUnlocked(KxDynamicStringRefW, KxDynamicStringRefW targetPath) = delete;
			void UpdateDispatcherIndexUnlocked(KxDynamicStringRefW requestedPath) = delete;
			bool UpdateDispatcherIndex(KxDynamicStringRefW, KxDynamicStringRefW targetPath) = delete;
			void UpdateDispatcherIndex(KxDynamicStringRefW requestedPath) = delete;
			bool TryDispatchRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) const = delete;

			bool IsINIFile(KxDynamicStringRefW requestedPath) const = delete;
			bool IsINIFileNonExistent(KxDynamicStringRefW requestedPath) const = delete;
			void AddINIFile(KxDynamicStringRefW requestedPath) = delete;

			void BuildDispatcherIndex() = delete;
			void SetDispatcherIndex(const TExternalDispatcherMap& index) = delete;
			template<class T> void SetDispatcherIndexT(const T& index) = delete;
			template<class T> void SetDispatcherIndex(const T& index) = delete;

			KxDynamicStringRefW GetWriteTarget() const = delete;
			bool SetWriteTarget(KxDynamicStringRefW writeTarget) = delete;

		protected:
			// IRequestDispatcher
			void DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath) override;

		public:
			MultiMirrorFS(Service& service, KxDynamicStringRefW mountPoint = {}, KxDynamicStringRefW source = {}, uint32_t flags = DefFlags);
			virtual ~MultiMirrorFS();

		protected:
			DWORD OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, KxVFS::Utility::StringSearcherHash& hashStore);
			NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
	};
}
