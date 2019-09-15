/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
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
			FileLogger(KxDynamicStringRefW filePath)
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
