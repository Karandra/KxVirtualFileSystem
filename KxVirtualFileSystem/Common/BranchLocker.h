/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"

namespace KxVFS
{
	class FileNode;
}

namespace KxVFS
{
	class BranchLockerData final
	{
		private:
			std::vector<FileNode*> m_LockedNodes;
			FileNode* m_Node = nullptr;

		public:
			BranchLockerData() = default;
			BranchLockerData(FileNode& node);

		public:
			bool IsStartNode(const FileNode& node) const noexcept
			{
				return m_Node == &node;
			}
			void AddLockedNode(FileNode& node)
			{
				m_LockedNodes.push_back(&node);
			}
			template<class TFunctor> void ForLockedNodes(TFunctor&& func) noexcept
			{
				for (auto it = m_LockedNodes.rbegin(); it != m_LockedNodes.rend(); ++it)
				{
					func(**it);
				}
			}
	};
}

namespace KxVFS
{
	class BranchSharedLocker final
	{
		private:
			BranchLockerData m_LockData;

		public:
			BranchSharedLocker(FileNode& node);
			BranchSharedLocker(BranchSharedLocker&& other) noexcept
			{
				*this = std::move(other);
			}
			BranchSharedLocker(const BranchSharedLocker&) = delete;
			~BranchSharedLocker();
			
		public:
			BranchSharedLocker& operator=(const BranchSharedLocker&) = delete;
			BranchSharedLocker& operator=(BranchSharedLocker&& other) noexcept
			{
				m_LockData = std::move(other.m_LockData);
				return *this;
			}
	};
}

namespace KxVFS
{
	class BranchExclusiveLocker final
	{
		private:
			BranchLockerData m_LockData;

		public:
			BranchExclusiveLocker(FileNode& node);
			BranchExclusiveLocker(BranchExclusiveLocker&& other)
			{
				*this = std::move(other);
			}
			BranchExclusiveLocker(const BranchExclusiveLocker&) = delete;
			~BranchExclusiveLocker();
			
		public:
			BranchExclusiveLocker& operator=(const BranchExclusiveLocker&) = delete;
			BranchExclusiveLocker& operator=(BranchExclusiveLocker&& other)
			{
				m_LockData = std::move(other.m_LockData);
				return *this;
			}
	};
}
