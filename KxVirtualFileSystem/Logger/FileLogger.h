#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility/FileHandle.h"
#include "ILogger.h"

namespace KxVFS
{
	class KxVFS_API FileLogger: public ILogger
	{
		protected:
			FileHandle m_Handle;

		public:
			FileLogger(FileHandle&& handle)
				:m_Handle(std::move(handle))
			{
			}
			FileLogger(DynamicStringRefW filePath)
				:m_Handle(filePath, AccessRights::GenericWrite, FileShare::Read, CreationDisposition::CreateAlways, FileAttributes::None)
			{
			}
			
		public:
			size_t LogString(Logger::InfoPack& infoPack) override;

		public:
			const FileHandle& GetHandle() const
			{
				return m_Handle;
			}
	};
}
