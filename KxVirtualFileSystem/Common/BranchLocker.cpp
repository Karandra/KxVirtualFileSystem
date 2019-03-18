/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "BranchLocker.h"
#include "FileNode.h"

namespace KxVFS
{
	BranchLockerData::BranchLockerData(FileNode& node)
		:m_Node(&node)
	{
		if (!node.IsRootNode())
		{
			m_LockedNodes.reserve(8);
		}
	}
}

namespace KxVFS
{
	BranchSharedLocker::BranchSharedLocker(FileNode& node)
		:m_LockData(node)
	{
		node.WalkToRoot([this](FileNode& node)
		{
			node.GetLock().AcquireShared();
			m_LockData.AddLockedNode(node);
			return true;
		});
	}
	BranchSharedLocker::~BranchSharedLocker()
	{
		m_LockData.ForLockedNodes([](FileNode& node)
		{
			node.m_Lock.ReleaseShared();
		});
	}
}

namespace KxVFS
{
	BranchExclusiveLocker::BranchExclusiveLocker(FileNode& node)
		:m_LockData(node)
	{
		node.WalkToRoot([this](FileNode& node)
		{
			if (m_LockData.IsStartNode(node))
			{
				node.GetLock().AcquireExclusive();
			}
			else
			{
				node.GetLock().AcquireShared();
			}
			m_LockData.AddLockedNode(node);

			return true;
		});
	}
	BranchExclusiveLocker::~BranchExclusiveLocker()
	{
		m_LockData.ForLockedNodes([this](FileNode& node)
		{
			if (m_LockData.IsStartNode(node))
			{
				node.m_Lock.ReleaseExclusive();
			}
			else
			{
				node.m_Lock.ReleaseShared();
			}
		});
	}
}
