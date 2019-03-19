/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Misc/IncludeWindows.h"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"

namespace KxVFS
{
	enum class FileAttributes: uint32_t
	{
		None = 0,

		Invalid = INVALID_FILE_ATTRIBUTES,
		Normal = FILE_ATTRIBUTE_NORMAL,
		Hidden = FILE_ATTRIBUTE_HIDDEN,
		Archive = FILE_ATTRIBUTE_ARCHIVE,
		Device = FILE_ATTRIBUTE_DEVICE,
		Directory = FILE_ATTRIBUTE_DIRECTORY,
		ReadOnly = FILE_ATTRIBUTE_READONLY,
		System = FILE_ATTRIBUTE_SYSTEM,
		Temporary = FILE_ATTRIBUTE_TEMPORARY,
		Compressed = FILE_ATTRIBUTE_COMPRESSED,
		Encrypted = FILE_ATTRIBUTE_ENCRYPTED,
		IntegrityStream = FILE_ATTRIBUTE_INTEGRITY_STREAM,
		NotContentIndexed = FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
		NoScrubData = FILE_ATTRIBUTE_NO_SCRUB_DATA,
		Offline = FILE_ATTRIBUTE_OFFLINE,
		RecallOnOpen = 0x40000, // FILE_ATTRIBUTE_RECALL_ON_OPEN is not defined for some reason
		ReparsePoint = FILE_ATTRIBUTE_REPARSE_POINT,
		SparseFile = FILE_ATTRIBUTE_SPARSE_FILE,
		Virtual = FILE_ATTRIBUTE_VIRTUAL,

		FlagDeleteOnClose = FILE_FLAG_DELETE_ON_CLOSE,
		FlagNoBuffering = FILE_FLAG_NO_BUFFERING,
		FlagOpenNoRecall = FILE_FLAG_OPEN_NO_RECALL,
		FlagOpenReparsePoint = FILE_FLAG_OPEN_REPARSE_POINT,
		FlagOverlapped = FILE_FLAG_OVERLAPPED,
		FlagBackupSemantics = FILE_FLAG_BACKUP_SEMANTICS,
		FlagPosixSemantics = FILE_FLAG_POSIX_SEMANTICS,
		FlagRandomAccess = FILE_FLAG_RANDOM_ACCESS,
		FlagSequentialScan = FILE_FLAG_SEQUENTIAL_SCAN,
		FlagSessionAware = FILE_FLAG_SESSION_AWARE,
		FlagWriteThrough = FILE_FLAG_WRITE_THROUGH,
	};
	KxVFS_AllowEnumBitwiseOp(FileAttributes);
	
	enum class FileFlags: uint32_t
	{
		DeleteOnClose = FILE_FLAG_DELETE_ON_CLOSE,
		NoBuffering = FILE_FLAG_NO_BUFFERING,
		OpenNoRecall = FILE_FLAG_OPEN_NO_RECALL,
		OpenReparsePoint = FILE_FLAG_OPEN_REPARSE_POINT,
		Overlapped = FILE_FLAG_OVERLAPPED,
		BackupSemantics = FILE_FLAG_BACKUP_SEMANTICS,
		PosixSemantics = FILE_FLAG_POSIX_SEMANTICS,
		RandomAccess = FILE_FLAG_RANDOM_ACCESS,
		SequentialScan = FILE_FLAG_SEQUENTIAL_SCAN,
		SessionAware = FILE_FLAG_SESSION_AWARE,
		WriteThrough = FILE_FLAG_WRITE_THROUGH,
	};
	KxVFS_AllowEnumBitwiseOp(FileFlags);
	
	enum class SecurityFlags: uint32_t
	{
		Anonymous = SECURITY_ANONYMOUS,
		Delegation = SECURITY_CONTEXT_TRACKING,
		Identification = SECURITY_IDENTIFICATION,
		Impersonation = SECURITY_IMPERSONATION,
		ContextTracking = SECURITY_CONTEXT_TRACKING,
		EffectiveOnly = SECURITY_EFFECTIVE_ONLY,
	};
	KxVFS_AllowEnumBitwiseOp(SecurityFlags);
	
	enum class FileShare: uint32_t
	{
		None = 0,
		All = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,

		Read = FILE_SHARE_READ,
		Write = FILE_SHARE_WRITE,
		Delete = FILE_SHARE_DELETE,
	};
	KxVFS_AllowEnumBitwiseOp(FileShare);
	
	enum class CreationDisposition: uint32_t
	{
		None = 0,

		CreateNew = CREATE_NEW,
		CreateAlways = CREATE_ALWAYS,
		OpenAlways = OPEN_ALWAYS,
		OpenExisting = OPEN_EXISTING,
		TruncateExisting = TRUNCATE_EXISTING,
	};
	KxVFS_AllowEnumBitwiseOp(CreationDisposition);
	
	enum class AccessRights: uint32_t
	{
		None = 0,

		// For a directory
		AddFile = FILE_ADD_FILE,
		AddSubDirectory = FILE_ADD_SUBDIRECTORY,
		DeleteChild = FILE_DELETE_CHILD,
		ListDirectory = FILE_LIST_DIRECTORY,
		Traverse = FILE_TRAVERSE,

		// For a file
		ReadEA = FILE_READ_EA,
		ReadData = FILE_READ_DATA,
		WriteData = FILE_WRITE_DATA,
		AppendData = FILE_APPEND_DATA,
		Execute = FILE_EXECUTE,
		CreatePipeInstance = FILE_CREATE_PIPE_INSTANCE,

		// Common
		ReadAttributes = FILE_READ_ATTRIBUTES,
		WriteAttributes = FILE_WRITE_ATTRIBUTES,

		Delete = DELETE,
		ReadControl = READ_CONTROL,
		WriteDAC = WRITE_DAC,
		WriteOwner = WRITE_OWNER,
		Synchronize = SYNCHRONIZE,
		SystemSecurity = ACCESS_SYSTEM_SECURITY,
		MaximumAllowed = MAXIMUM_ALLOWED,

		GenericRead = GENERIC_READ,
		GenericWrite = GENERIC_WRITE,
		GenericExecute = GENERIC_EXECUTE,
		GenericAll = GENERIC_ALL,
		
		FileGenericRead = FILE_GENERIC_READ,
		FileGenericWrite = FILE_GENERIC_WRITE,
		FileGenericExecute = FILE_GENERIC_EXECUTE,

		StandardRead = STANDARD_RIGHTS_READ,
		StandardWrite = STANDARD_RIGHTS_WRITE,
		StandardExecute = STANDARD_RIGHTS_EXECUTE,
		StandardAll = STANDARD_RIGHTS_ALL,
		StandardRequired = STANDARD_RIGHTS_REQUIRED,
		SpecificAll = SPECIFIC_RIGHTS_ALL,
	};
	KxVFS_AllowEnumBitwiseOp(AccessRights);

	enum class ReparsePointTags: uint32_t
	{
		None = 0,

		CSV = IO_REPARSE_TAG_CSV,
		DEDUP = IO_REPARSE_TAG_DEDUP,
		DFS = IO_REPARSE_TAG_DFS,
		DFSR = IO_REPARSE_TAG_DFSR,
		HSM = IO_REPARSE_TAG_HSM,
		HSM2 = IO_REPARSE_TAG_HSM2,
		NFS = IO_REPARSE_TAG_NFS,
		SIS = IO_REPARSE_TAG_SIS,
		WIM = IO_REPARSE_TAG_WIM,
		Symlink = IO_REPARSE_TAG_SYMLINK,
		MountPoint = IO_REPARSE_TAG_MOUNT_POINT,
		ReservedOne = IO_REPARSE_TAG_RESERVED_ONE,
		ReservedZero = IO_REPARSE_TAG_RESERVED_ZERO,
		ReservedRange = IO_REPARSE_TAG_RESERVED_RANGE,
	};
	KxVFS_AllowEnumBitwiseOp(ReparsePointTags);
}
