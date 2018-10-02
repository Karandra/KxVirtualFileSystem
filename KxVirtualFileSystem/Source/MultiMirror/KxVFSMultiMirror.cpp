/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem.h"
#include "KxVFSBase.h"
#include "KxVFSService.h"
#include "KxVFSMultiMirror.h"
#include "Utility/KxVFSUtility.h"
#include "Utility/KxVFSFileHandle.h"
#include "Utility/KxFileFinder.h"

// KxVFSIDispatcher
KxDynamicString KxVFSMultiMirror::GetTargetPath(const WCHAR* requestedPath)
{
	// Search file in write target and in all virtual folders
	KxDynamicString inWriteTarget = MakeFilePath(GetSourceRef(), requestedPath);
	if (!KxVFSUtility::IsExist(inWriteTarget))
	{
		KxDynamicString targetPath;
		for (auto i = GetPaths().rbegin(); i != GetPaths().rend(); ++i)
		{
			targetPath = MakeFilePath(*i, requestedPath);
			if (KxVFSUtility::IsExist(targetPath))
			{
				return targetPath;
			}
		}
	}
	return inWriteTarget;
}

KxVFSMultiMirror::KxVFSMultiMirror(KxVFSService* vfsService, const WCHAR* mountPoint, const WCHAR* source, ULONG falgs, ULONG requestTimeout)
	:KxVFSConvergence(vfsService, mountPoint, source, falgs, requestTimeout)
{
}
KxVFSMultiMirror::~KxVFSMultiMirror()
{
}

//////////////////////////////////////////////////////////////////////////
DWORD KxVFSMultiMirror::OnFindFilesAux(const KxDynamicString& path, EvtFindFiles& eventInfo, KxVFSUtility::StringSearcherHash& hashStore)
{
	DWORD errorCode = ERROR_NO_MORE_FILES;

	WIN32_FIND_DATAW findData = {0};
	HANDLE findHandle = FindFirstFileW(path, &findData);
	if (findHandle != INVALID_HANDLE_VALUE)
	{
		KxDynamicString fileName;
		do
		{
			// Hash only lowercase version of name
			fileName.assign(findData.cFileName);
			fileName.make_lower();

			// If this file is not found already
			size_t hashValue = KxVFSUtility::HashString(fileName);
			if (hashStore.emplace(hashValue).second)
			{
				eventInfo.FillFindData(&eventInfo, &findData);

				// Save this path into dispatcher index.
				KxDynamicString requestedPath(eventInfo.PathName);
				if (!requestedPath.empty() && requestedPath.back() != TEXT('\\'))
				{
					requestedPath += TEXT('\\');
				}
				requestedPath += findData.cFileName;

				// Remove '*' at end.
				KxDynamicString targetPath(path);
				targetPath.erase(targetPath.length() - 1, 1);
				targetPath += findData.cFileName;
			}
		}
		while (FindNextFileW(findHandle, &findData));
		errorCode = GetLastError();

		FindClose(findHandle);
	}
	return errorCode;
}
NTSTATUS KxVFSMultiMirror::OnFindFiles(EvtFindFiles& eventInfo)
{
	auto AppendAsterix = [](KxDynamicString& path) -> KxDynamicString&
	{
		if (!path.empty())
		{
			if (path.back() != TEXT('\\'))
			{
				path += TEXT('\\');
			}
			path += TEXT('*');
		}
		return path;
	};

	DWORD errorCode = 0;
	KxVFSUtility::StringSearcherHash foundPaths = {KxVFSUtility::HashString(L"."), KxVFSUtility::HashString(L"..")};

	// Find everything in source first as it have highest priority
	KxDynamicString writeTarget = MakeFilePath(GetSourceRef(), eventInfo.PathName);
	AppendAsterix(writeTarget);

	errorCode = OnFindFilesAux(writeTarget, eventInfo, foundPaths);

	// Then in other folders
	for (auto it = GetPaths().rbegin(); it != GetPaths().rend(); ++it)
	{
		KxDynamicString path = MakeFilePath(*it, eventInfo.PathName);
		errorCode = OnFindFilesAux(AppendAsterix(path), eventInfo, foundPaths);
	}

	if (errorCode != ERROR_NO_MORE_FILES)
	{
		return GetNtStatusByWin32LastErrorCode();
	}
	return STATUS_SUCCESS;
}
