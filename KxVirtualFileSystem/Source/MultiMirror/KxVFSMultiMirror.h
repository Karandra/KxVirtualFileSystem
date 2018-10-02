/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSIDispatcher.h"
#include "KxVFSISearchDispatcher.h"
#include "Convergence/KxVFSConvergence.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxDynamicString.h"
#include "Utility/KxVFSContext.h"
class KxVFSService;

class KxVFS_API KxVFSMultiMirror: public KxVFSConvergence
{
	private:
		using RedirectionPathsListT = std::vector<std::wstring>;

	private:
		bool UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath, const KxDynamicString& targetPath) = delete;
		void UpdateDispatcherIndexUnlocked(const KxDynamicString& requestedPath) = delete;
		bool UpdateDispatcherIndex(const KxDynamicString& requestedPath, const KxDynamicString& targetPath) = delete;
		void UpdateDispatcherIndex(const KxDynamicString& requestedPath) = delete;
		KxDynamicString TryDispatchRequest(const KxDynamicString& requestedPath) const = delete;

		// Non-existent INI-files
		bool IsINIFile(const KxDynamicStringRef& requestedPath) const = delete;
		bool IsINIFileNonExistent(const KxDynamicStringRef& requestedPath) const = delete;
		void AddINIFile(const KxDynamicStringRef& requestedPath) = delete;

		bool CanDeleteInVirtualFolder() const = delete;
		bool SetCanDeleteInVirtualFolder(bool value) = delete;

		void BuildDispatcherIndex() = delete;
		void SetDispatcherIndex(const ExternalDispatcherIndexT& index) = delete;
		template<class T> void SetDispatcherIndexT(const T& index) = delete;
		template<class T> void SetDispatcherIndex(const T& index) = delete;

		const WCHAR* GetWriteTarget() const = delete;
		const KxDynamicStringRef GetWriteTargetRef() const = delete;
		bool SetWriteTarget(const WCHAR* writeTarget) = delete;

	protected:
		// KxVFSIDispatcher
		virtual KxDynamicString GetTargetPath(const WCHAR* requestedPath) override;

	public:
		KxVFSMultiMirror(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* source, ULONG falgs = DefFlags, ULONG requestTimeout = DefRequestTimeout);
		virtual ~KxVFSMultiMirror();

	protected:
		DWORD OnFindFilesAux(const KxDynamicString& path, EvtFindFiles& eventInfo, KxVFSUtility::StringSearcherHash& hashStore);
		virtual NTSTATUS OnFindFiles(EvtFindFiles& eventInfo) override;
};
