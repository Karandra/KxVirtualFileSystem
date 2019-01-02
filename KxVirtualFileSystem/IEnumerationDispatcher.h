/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "IncludeDokan.h"
#include "Utility.h"

namespace KxVFS
{
	class KxVFS_API IEnumerationDispatcher
	{
		public:
			using TEnumerationVector = std::vector<WIN32_FIND_DATA>;

		protected:
			void SendEnumerationVector(EvtFindFiles* eventInfo, const TEnumerationVector& items)
			{
				for (const WIN32_FIND_DATA& item: items)
				{
					eventInfo->FillFindData(eventInfo, const_cast<WIN32_FIND_DATA*>(&item));
				}
			}
		
			virtual TEnumerationVector* GetEnumerationVector(KxDynamicStringRefW requestedPath) = 0;
			virtual TEnumerationVector& CreateEnumerationVector(KxDynamicStringRefW requestedPath) = 0;
			virtual void InvalidateEnumerationVector(KxDynamicStringRefW requestedPath) = 0;
	};
}
