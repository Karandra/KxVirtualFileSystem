#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility.h"
#include "FileItem.h"

namespace KxVFS
{
	class KxVFS_API IFileFinder
	{
		protected:
			virtual bool OnFound(const FileItem& foundItem) = 0;

			DynamicStringW Normalize(DynamicStringRefW source, bool start, bool end) const;
			DynamicStringW ConstructSearchQuery(DynamicStringRefW source, DynamicStringRefW filter) const;
			DynamicStringW ExtrctSourceFromSearchQuery(DynamicStringRefW searchQuery) const;

		public:
			IFileFinder() = default;
			virtual ~IFileFinder() = default;

		public:
			virtual bool IsOK() const = 0;
			virtual bool IsCanceled() const = 0;

			virtual bool Run() = 0;
			virtual FileItem FindNext() = 0;
	};
}
