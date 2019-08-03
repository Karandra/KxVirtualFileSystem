/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Common/FileNode.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/FileSystemService.h"
#include "ILogger.h"

namespace KxVFS
{
	bool ILogger::HasPrimaryLogger()
	{
		return FileSystemService::GetInstance() != nullptr;
	}
	ILogger& ILogger::Get()
	{
		return FileSystemService::GetInstance()->GetLogger();
	}

	KxDynamicStringRefW ILogger::GetLogLevelName(LogLevel level) const
	{
		switch (level)
		{
			case LogLevel::Info:
			{
				return L"Info";
			}
			case LogLevel::Warning:
			{
				return L"Warning";
			}
			case LogLevel::Error:
			{
				return L"Error";
			}
			case LogLevel::Fatal:
			{
				return L"Fatal";
			}
		};
		return L"None";
	}
	KxDynamicStringW ILogger::FormatInfoPack(const Logger::InfoPack& infoPack) const
	{
		KxDynamicStringW text = KxDynamicStringW::Format(L"[%s][Thread: %u] %s",
														 GetLogLevelName(infoPack.GetLevel()).data(),
														 infoPack.GetThreadID(),
														 infoPack.GetString().data()
		);

		if (const IFileSystem* fileSystem = infoPack.GetFileSystem())
		{
			KxDynamicStringW mountPoint = fileSystem->GetMountPoint();
			text += KxDynamicStringW::Format(L"\r\n\t[File System]: %s", mountPoint.data());
		}
		if (const FileNode* fileNode = infoPack.GetFileNode())
		{
			KxDynamicStringW fullPath = fileNode->GetFullPath();
			text += KxDynamicStringW::Format(L"\r\n\t[File Node]: %s", fullPath.data());
		}
		text += L"\r\n";

		return text;
	}
}
