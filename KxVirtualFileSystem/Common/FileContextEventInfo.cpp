/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileContextEventInfo.h"

namespace KxVFS
{
	void FileContextEventInfo::Assign(const EvtCreateFile& eventInfo)
	{
		auto[requestAttributes, creationDisposition, genericDesiredAccess] = IFileSystem::MapKernelToUserCreateFileFlags(eventInfo);

		m_CreationDisposition = creationDisposition;
		m_Attributes = requestAttributes;
		m_DesiredAccess = genericDesiredAccess;
		m_ShareMode = FromInt<FileShare>(eventInfo.ShareAccess);
		m_KernelCreationOptions = FromInt<KernelFileOptions>(eventInfo.CreateOptions);
		m_OriginProcessID = eventInfo.DokanFileInfo->ProcessId;
	}
}
