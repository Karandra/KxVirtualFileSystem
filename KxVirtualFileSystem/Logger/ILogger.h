#pragma once
#include "KxVirtualFileSystem/Common.hpp"
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
			DynamicStringW String;
			const IFileSystem* FileSystem = nullptr;
			const FileNode* FileNode = nullptr;
			const uint32_t ThreadID = 0;
			LogLevel LogLevel = LogLevel::Info;
		
		public:
			InfoPack() = default;
			InfoPack(KxVFS::LogLevel level,
					 KxVFS::DynamicStringW text,
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
			DynamicStringRefW GetLogLevelName(LogLevel level) const;
			DynamicStringW FormatInfoPack(const Logger::InfoPack& infoPack) const;

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
	using namespace KxVFS;	\
	if (ILogger::IsLogEnabled())	\
	{	\
		ILogger::Get().Log(level, format, __VA_ARGS__);	\
	}	\
}	\
