/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/AbstractFS.h"
#include "KxVirtualFileSystem/Service.h"
#include "KxVirtualFileSystem/Utility.h"
#include "MultiMirrorFS.h"

// IRequestDispatcher
namespace KxVFS
{
	void MultiMirrorFS::DispatchLocationRequest(KxDynamicStringRefW requestedPath, KxDynamicStringW& targetPath)
	{
		// Search file in write target and in all virtual folders
		KxDynamicStringW inWriteTarget;
		MakeFilePath(inWriteTarget, GetSource(), requestedPath);
		if (!Utility::IsExist(inWriteTarget))
		{
			KxDynamicStringW targetPath;
			for (auto i = GetVirtualFolders().rbegin(); i != GetVirtualFolders().rend(); ++i)
			{
				MakeFilePath(targetPath, *i, requestedPath);
				if (Utility::IsExist(targetPath))
				{
					return;
				}
			}
		}
		targetPath = inWriteTarget;
	}
}

namespace KxVFS
{
	MultiMirrorFS::MultiMirrorFS(Service& service, KxDynamicStringRefW mountPoint, KxDynamicStringRefW source, uint32_t flags)
		:ConvergenceFS(service, mountPoint, source, flags)
	{
	}
	MultiMirrorFS::~MultiMirrorFS()
	{
	}

	DWORD MultiMirrorFS::OnFindFilesAux(const KxDynamicStringW& path, EvtFindFiles& eventInfo, Utility::StringSearcherHash& hashStore)
	{
		DWORD errorCode = ERROR_NO_MORE_FILES;

		WIN32_FIND_DATAW findData = {0};
		HANDLE findHandle = FindFirstFileW(path, &findData);
		if (findHandle != INVALID_HANDLE_VALUE)
		{
			KxDynamicStringW fileName;
			do
			{
				// Hash only lowercase version of name
				fileName.assign(findData.cFileName);
				fileName.make_lower();

				// If this file is not found already
				size_t hashValue = Utility::HashString(fileName);
				if (hashStore.emplace(hashValue).second)
				{
					eventInfo.FillFindData(&eventInfo, &findData);

					// Save this path into dispatcher index.
					KxDynamicStringW requestedPath(eventInfo.PathName);
					if (!requestedPath.empty() && requestedPath.back() != TEXT('\\'))
					{
						requestedPath += TEXT('\\');
					}
					requestedPath += findData.cFileName;

					// Remove '*' at end.
					KxDynamicStringW targetPath(path);
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
	NTSTATUS MultiMirrorFS::OnFindFiles(EvtFindFiles& eventInfo)
	{
		auto AppendAsterix = [](KxDynamicStringW& path) -> KxDynamicStringW&
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
		Utility::StringSearcherHash foundPaths = {Utility::HashString(L"."), Utility::HashString(L"..")};

		// Find everything in source first as it have highest priority
		KxDynamicStringW writeTarget;
		MakeFilePath(writeTarget, GetSource(), eventInfo.PathName);
		AppendAsterix(writeTarget);

		errorCode = OnFindFilesAux(writeTarget, eventInfo, foundPaths);

		// Then in other folders
		for (auto it = GetVirtualFolders().rbegin(); it != GetVirtualFolders().rend(); ++it)
		{
			KxDynamicStringW path;
			MakeFilePath(path, *it, eventInfo.PathName);
			errorCode = OnFindFilesAux(AppendAsterix(path), eventInfo, foundPaths);
		}

		if (errorCode != ERROR_NO_MORE_FILES)
		{
			return GetNtStatusByWin32LastErrorCode();
		}
		return STATUS_SUCCESS;
	}
}
