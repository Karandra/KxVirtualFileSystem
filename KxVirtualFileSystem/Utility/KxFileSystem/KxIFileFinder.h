/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "KxFileItem.h"

namespace KxVFS::Utility
{
	class KxVFS_API KxIFileFinder
	{
		protected:
			virtual bool OnFound(const KxFileItem& foundItem) = 0;

			KxDynamicStringW Normalize(KxDynamicStringRefW source, bool start, bool end) const;
			KxDynamicStringW ConstructSearchQuery(KxDynamicStringRefW source, KxDynamicStringRefW filter) const;
			KxDynamicStringW ExtrctSourceFromSearchQuery(KxDynamicStringRefW searchQuery) const;

		public:
			KxIFileFinder() = default;
			virtual ~KxIFileFinder() = default;

		public:
			virtual bool IsOK() const = 0;
			virtual bool IsCanceled() const = 0;

			virtual bool Run() = 0;
			virtual KxFileItem FindNext() = 0;
	};
}
