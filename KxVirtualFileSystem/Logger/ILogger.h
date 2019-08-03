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
		private:
			KxDynamicStringW m_String;
			const IFileSystem* m_FileSystem = nullptr;
			const FileNode* m_FileNode = nullptr;
			uint32_t m_ThreadID = 0;
			LogLevel m_LogLevel = LogLevel::Info;

		public:
			InfoPack(LogLevel level,
					 KxDynamicStringW&& text,
					 const IFileSystem* fileSystem = nullptr,
					 const FileNode* fileNode = nullptr
			)
				:m_String(std::move(text)), m_FileSystem(fileSystem), m_FileNode(fileNode), m_LogLevel(level)
			{
				m_ThreadID = ::GetCurrentThreadId();
			}
			
		public:
			const KxDynamicStringW& GetString() const
			{
				return m_String;
			}
			const IFileSystem* GetFileSystem() const
			{
				return m_FileSystem;
			}
			const FileNode* GetFileNode() const
			{
				return m_FileNode;
			}
			uint32_t GetThreadID() const
			{
				return m_ThreadID;
			}
			LogLevel GetLevel() const
			{
				return m_LogLevel;
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

		protected:
			KxDynamicStringRefW GetLogLevelName(LogLevel level) const;
			KxDynamicStringW FormatInfoPack(const Logger::InfoPack& infoPack) const;

		public:
			virtual ~ILogger() = default;

		public:
			virtual size_t LogString(const Logger::InfoPack& infoPack) = 0;

		public:
			template<class... Args> size_t Log(LogLevel level, const wchar_t* format, Args&& ... arg)
			{
				return LogString(Logger::InfoPack(level, KxDynamicStringW::Format(format, std::forward<Args>(arg)...)));
			}
	};
}

#if KxVFS_DEBUG_ENABLE_LOG
#define KxVFS_Log ILogger::Get().Log
#else
#define KxVFS_Log
#endif
