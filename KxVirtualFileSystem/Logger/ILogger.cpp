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

namespace
{
	static bool g_LogEnabled = false;
}

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

	bool ILogger::IsLogEnabled()
	{
		return g_LogEnabled;
	}
	void ILogger::EnableLog(bool value)
	{
		g_LogEnabled = value;
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
		KxDynamicStringW text = Utility::FormatString(L"[%1][Thread:%2] %3",
													  GetLogLevelName(infoPack.LogLevel),
													  infoPack.ThreadID,
													  infoPack.String
		);

		if (infoPack.FileSystem)
		{
			KxDynamicStringW mountPoint = infoPack.FileSystem->GetMountPoint();
			text += Utility::FormatString(L"\r\n\t[File System]: %1", mountPoint.data());
		}
		if (infoPack.FileNode)
		{
			KxDynamicStringW fullPath = infoPack.FileNode->GetFullPath();
			text += Utility::FormatString(L"\r\n\t[File Node]: %1", fullPath.data());
		}
		text += L"\r\n";

		return text;
	}
}
