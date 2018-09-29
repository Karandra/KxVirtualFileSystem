/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeDokan.h"
#include "Utility/KxDynamicString.h"

class KxVFS_API KxVFSISearchDispatcher
{
	public:
		using SearchDispatcherVectorT = std::vector<WIN32_FIND_DATA>;

	protected:
		void SendDispatcherVector(KxVFSEvents::EvtFindFiles* eventInfo, const SearchDispatcherVectorT& items)
		{
			for (const WIN32_FIND_DATA& item: items)
			{
				eventInfo->FillFindData(eventInfo, const_cast<WIN32_FIND_DATA*>(&item));
			}
		}
		
		virtual SearchDispatcherVectorT* GetSearchDispatcherVector(const KxDynamicString& requestedPath) = 0;
		virtual SearchDispatcherVectorT* CreateSearchDispatcherVector(const KxDynamicString& requestedPath) = 0;
		virtual void InvalidateSearchDispatcherVector(const KxDynamicString& requestedPath) = 0;
};
