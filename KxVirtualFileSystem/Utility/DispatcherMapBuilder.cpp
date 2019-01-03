/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "DispatcherMapBuilder.h"

namespace KxVFS::Utility
{
	void DispatcherMapBuilder::ExtractRequestPath(KxDynamicStringW& requestPath, KxDynamicStringRefW virtualFolder, const KxDynamicStringW& targetPath)
	{
		requestPath = targetPath;

		size_t eraseOffset = 0;
		if (requestPath.length() >= virtualFolder.length() && requestPath[virtualFolder.length()] == L'\\')
		{
			eraseOffset = 1;
		}
		requestPath.erase(0, virtualFolder.length() + eraseOffset);
	}
	void DispatcherMapBuilder::BuildTreeBranch(TVector& directories, KxDynamicStringRefW virtualFolder, KxDynamicStringRefW path)
	{
		KxFileFinder finder(path, L"*");
		for (KxFileItem item = finder.FindNext(); item.IsOK(); item = finder.FindNext())
		{
			if (item.IsNormalItem())
			{
				const KxDynamicStringW fullPath = item.GetFullPath();

				KxDynamicStringW requestPath;
				ExtractRequestPath(requestPath, virtualFolder, fullPath);
				OnPath(virtualFolder, requestPath, item.GetFullPathWithNS());

				if (item.IsDirectory())
				{
					directories.push_back(fullPath);
				}
			}
		}
	}

	void DispatcherMapBuilder::Run(KxDynamicStringRefW virtualFolder)
	{
		// Build top level
		TVector directories;
		BuildTreeBranch(directories, virtualFolder, virtualFolder);

		// Build subdirectories
		while (!directories.empty())
		{
			TVector roundDirectories;
			roundDirectories.reserve(directories.size());

			for (const KxDynamicStringW& path: directories)
			{
				BuildTreeBranch(roundDirectories, virtualFolder, path);
			}
			directories = std::move(roundDirectories);
		}
	}
}
