/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS
{
	class KxVFS_API IFileSystem;
	class KxVFS_API FileSystemService;
	class KxVFS_API FileNode;
}

namespace KxVFS
{
	enum class LogLevel
	{
		Info,
		Warning,
		Error,
		Fatal
	};
}

namespace KxVFS::Logger
{
	class InfoPack final
	{
		public:
			KxDynamicStringW String;
			const IFileSystem* FileSystem = nullptr;
			const FileNode* FileNode = nullptr;
			const uint32_t ThreadID = 0;
			LogLevel LogLevel = LogLevel::Info;
		
		public:
			InfoPack() = default;
			InfoPack(KxVFS::LogLevel level,
					 KxVFS::KxDynamicStringW text,
					 const KxVFS::IFileSystem* fileSystem = nullptr,
					 const KxVFS::FileNode* fileNode = nullptr
			)
				:String(std::move(text)), FileSystem(fileSystem),
				FileNode(fileNode),
				LogLevel(level),
				ThreadID(::GetCurrentThreadId())
			{
			}
	};
}

namespace KxVFS
{
	class KxVFS_API ILogger
	{
		public:
			static bool HasPrimaryLogger();
			static ILogger& Get();

			static bool IsLogEnabled();
			static void EnableLog(bool value = true);

		protected:
			KxDynamicStringRefW GetLogLevelName(LogLevel level) const;
			KxDynamicStringW FormatInfoPack(const Logger::InfoPack& infoPack) const;

		public:
			virtual ~ILogger() = default;

		public:
			virtual size_t LogString(Logger::InfoPack& infoPack) = 0;

			template<class... Args> size_t Log(LogLevel level, const wchar_t* format, Args&& ... arg)
			{
				Logger::InfoPack logInfo(level, Utility::FormatString(format, std::forward<Args>(arg)...));
				return LogString(logInfo);
			}
	};
}

#define KxVFS_Log(level, format, ...)	\
if constexpr(KxVFS::Setup::EnableLog)	\
{	\
	if (KxVFS::ILogger::IsLogEnabled())	\
	{	\
		KxVFS::ILogger::Get().Log(level, format, __VA_ARGS__);	\
	}	\
}	\
