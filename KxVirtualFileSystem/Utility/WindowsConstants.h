/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/IncludeWindows.h"

namespace KxVFS
{
	enum class FileAttributes: uint32_t
	{
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
	};
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
	enum class SecurityFlags: uint32_t
	{
		Anonymous = SECURITY_ANONYMOUS,
		Delegation = SECURITY_CONTEXT_TRACKING,
		Identification = SECURITY_IDENTIFICATION,
		Impersonation = SECURITY_IMPERSONATION,
		ContextTracking = SECURITY_CONTEXT_TRACKING,
		EffectiveOnly = SECURITY_EFFECTIVE_ONLY,
	};
	enum class FileShare: uint32_t
	{
		None = 0,
		Read = FILE_SHARE_READ,
		Write = FILE_SHARE_WRITE,
		Delete = FILE_SHARE_DELETE,
	};
	enum class CreationDisposition: uint32_t
	{
		CreateNew = CREATE_NEW,
		CreateAlways = CREATE_ALWAYS,
		OpenAlways = OPEN_ALWAYS,
		OpenExisting = OPEN_EXISTING,
		TruncateExisting = TRUNCATE_EXISTING,
	};
	enum class FileAccess: uint32_t
	{
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
		
		// Combinations
		AllAccess = FILE_ALL_ACCESS,
		StandardRightsRead = STANDARD_RIGHTS_READ,
		StandardRightsWrite = STANDARD_RIGHTS_WRITE,
	};
	enum class GenericAccess: uint32_t
	{
		Read = GENERIC_READ,
		Write = GENERIC_WRITE,
		Execute = GENERIC_EXECUTE,
		All = GENERIC_ALL,
	};
}
