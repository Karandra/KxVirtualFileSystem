#include "stdafx.h"
#include "KxVFS/Common/FileNode.h"
#include "KxVFS/IFileSystem.h"
#include "KxVFS/FileSystemService.h"
#include "ILogger.h"

namespace
{
	bool g_LogEnabled = false;
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

	DynamicStringRefW ILogger::GetLogLevelName(LogLevel level) const
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
	DynamicStringW ILogger::FormatInfoPack(const Logger::InfoPack& infoPack) const
	{
		DynamicStringW text = Utility::FormatString(L"[%1][Thread:%2] %3",
													  GetLogLevelName(infoPack.LogLevel),
													  infoPack.ThreadID,
													  infoPack.String
		);

		if (infoPack.FileSystem)
		{
			DynamicStringW mountPoint = infoPack.FileSystem->GetMountPoint();
			text += Utility::FormatString(L"\r\n\t[File System]: %1", mountPoint.data());
		}
		if (infoPack.FileNode)
		{
			DynamicStringW fullPath = infoPack.FileNode->GetFullPath();
			text += Utility::FormatString(L"\r\n\t[File Node]: %1", fullPath.data());
		}
		text += L"\r\n";

		return text;
	}
}
