/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"

namespace KxVFS::Utility
{
	class KxVFS_API DispatcherMapBuilder
	{
		private:
			using TVector = std::vector<KxDynamicStringW>;

		private:
			void ExtractRequestPath(KxDynamicStringW& requestPath, KxDynamicStringRefW virtualFolder, const KxDynamicStringW& targetPath);;
			void BuildTreeBranch(TVector& directories, KxDynamicStringRefW virtualFolder, KxDynamicStringRefW path);;

		protected:
			virtual void OnPath(KxDynamicStringRefW virtualFolder, KxDynamicStringRefW requestPath, KxDynamicStringRefW targetPath) = 0;

		public:
			virtual ~DispatcherMapBuilder() = default;

		public:
			void Run(KxDynamicStringRefW virtualFolder);
	};
}

namespace KxVFS::Utility
{
	template<class TFunctor>
	class SimpleDispatcherMapBuilder: public DispatcherMapBuilder
	{
		private:
			TFunctor m_Functor;

		protected:
			void OnPath(KxDynamicStringRefW virtualFolder, KxDynamicStringRefW requestPath, KxDynamicStringRefW targetPath) override
			{
				(void)std::invoke(m_Functor, virtualFolder, requestPath, targetPath);
			}

		public:
			SimpleDispatcherMapBuilder(TFunctor&& func)
				:m_Functor(std::move(func))
			{
			}
			SimpleDispatcherMapBuilder(const TFunctor& func)
				:m_Functor(func)
			{
			}
	};
}
