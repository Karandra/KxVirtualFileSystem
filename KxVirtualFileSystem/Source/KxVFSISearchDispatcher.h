#pragma once
#include "KxVirtualFileSystem.h"
#include "KxVFSIncludeDokan.h"
#include "Utility/KxDynamicString.h"

class KxVFS_API KxVFSISearchDispatcher
{
	public:
		using SearchDispatcherVectorT = std::vector<WIN32_FIND_DATA>;

	protected:
		void SendDispatcherVector(DOKAN_FIND_FILES_EVENT* pEventInfo, const SearchDispatcherVectorT& tItems)
		{
			for (const WIN32_FIND_DATA& tItem: tItems)
			{
				pEventInfo->FillFindData(pEventInfo, const_cast<WIN32_FIND_DATA*>(&tItem));
			}
		}
		
		virtual SearchDispatcherVectorT* GetSearchDispatcherVector(const KxDynamicString& sRequestedPath) =0;
		virtual SearchDispatcherVectorT* CreateSearchDispatcherVector(const KxDynamicString& sRequestedPath) = 0;
		virtual void InvalidateSearchDispatcherVector(const KxDynamicString& sRequestedPath) = 0;
};
