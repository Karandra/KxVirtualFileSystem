#include "stdafx.h"
#include "KxVirtualFileSystem/IFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileContextEventInfo.h"

namespace KxVFS
{
	void FileContextEventInfo::Assign(const EvtCreateFile& eventInfo) noexcept
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
