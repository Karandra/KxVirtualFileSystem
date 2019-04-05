/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileNode.h"

namespace KxVFS
{
	FileNode* FileNode::NavigateToElement(FileNode& rootNode, KxDynamicStringRefW relativePath, NavigateTo type, FileNode*& lastScanned)
	{
		if ((type == NavigateTo::Folder || type == NavigateTo::Any) && IsRequestToRootNode(relativePath))
		{
			return &rootNode;
		}

		lastScanned = &rootNode;
		if (rootNode.HasChildren())
		{
			auto ScanChildren = [&lastScanned](FileNode& scannedNode, KxDynamicStringRefW folderName) -> FileNode*
			{
				auto it = scannedNode.m_Children.find(folderName);
				if (it != scannedNode.m_Children.end())
				{
					lastScanned = it->second.get();
					return lastScanned;
				}
				return nullptr;
			};
			auto StripQuotes = [](KxDynamicStringRefW folderName)
			{
				if (!folderName.empty() && folderName.front() == L'"')
				{
					folderName.remove_prefix(1);
				}
				if (!folderName.empty() && folderName.back() == L'"')
				{
					folderName.remove_suffix(1);
				}
				return folderName;
			};

			FileNode* finalNode = nullptr;
			Utility::String::SplitBySeparator(relativePath, L"\\", [&ScanChildren, &StripQuotes, &finalNode, &rootNode](KxDynamicStringRefW folderName)
			{
				finalNode = ScanChildren(finalNode ? *finalNode : rootNode, StripQuotes(folderName));
				return finalNode != nullptr;
			});

			if (finalNode)
			{
				lastScanned = finalNode->GetParent();
			}
			if (finalNode == nullptr || (type == NavigateTo::Folder && !finalNode->IsDirectory()) || (type == NavigateTo::File && !finalNode->IsFile()))
			{
				return nullptr;
			}
			return finalNode;
		}
		return nullptr;
	}

	bool FileNode::IsRequestToRootNode(KxDynamicStringRefW relativePath)
	{
		return relativePath.empty() || relativePath == L"\\" || relativePath == L"/";
	}
	size_t FileNode::HashFileName(KxDynamicStringRefW name)
	{
		return Utility::Comparator::StringHashNoCase()(name);
	}

	KxDynamicStringW FileNode::ConstructPath(PathParts options) const
	{
		KxDynamicStringW fullPath = options & PathParts::Namespace ? Utility::GetLongPathPrefix() : KxNullDynamicStringW;
		if (options & PathParts::BaseDirectory)
		{
			fullPath += m_VirtualDirectory;
			fullPath += L'\\';
		}

		if (options & PathParts::RelativePath)
		{
			KxDynamicStringW relativePath;
			WalkToRoot([this, &relativePath](const FileNode& node)
			{
				if (&node != this)
				{
					KxDynamicStringW name = node.GetName();
					name += L'\\';
					name += relativePath;
					relativePath = name;
				}
				return true;
			});
			fullPath += relativePath;
		}

		if (options & PathParts::Name)
		{
			fullPath += GetName();
		}
		else
		{
			// Remove trailing slash
			fullPath.erase(fullPath.length() - 1, 1);
		}
		return fullPath;
	}
	void FileNode::UpdateFileTree(KxDynamicStringRefW searchPath, bool queryShortNames)
	{
		m_VirtualDirectory = searchPath;
		ClearChildren();

		auto BuildTreeBranch = [this, queryShortNames](FileNode::RefVector& directories, KxDynamicStringRefW path, FileNode& treeNode, FileNode* parentNode)
		{
			KxFileFinder finder(path, L"*");
			finder.QueryShortNames(queryShortNames);
			for (KxFileItem item = finder.FindNext(); item.IsOK(); item = finder.FindNext())
			{
				if (item.IsNormalItem())
				{
					FileNode& node = treeNode.AddChild(std::make_unique<FileNode>(item, parentNode));
					if (node.IsDirectory())
					{
						directories.emplace_back(&node);
					}
				}
			}
		};

		// Build top level
		FileNode::RefVector directories;
		BuildTreeBranch(directories, searchPath, *this, this);

		// Build subdirectories
		while (!directories.empty())
		{
			FileNode::RefVector roundDirectories;
			roundDirectories.reserve(directories.size());

			for (FileNode* node: directories)
			{
				BuildTreeBranch(roundDirectories, node->GetFullPath(), *node, node);
			}
			directories = std::move(roundDirectories);
		}
	}
	void FileNode::MakeNull()
	{
		ClearChildren();
		m_Item = {};
		m_VirtualDirectory = {};
		m_Parent = nullptr;
	}

	const FileNode* FileNode::WalkTree(const TreeWalker& func) const
	{
		std::function<const FileNode*(const FileNode::Map&)> Recurse;
		Recurse = [&Recurse, &func](const FileNode::Map& children) -> const FileNode*
		{
			for (const auto& [name, node]: children)
			{
				if (!func(*node))
				{
					return node.get();
				}

				if (node->HasChildren())
				{
					Recurse(node->GetChildren());
				}
			}
			return nullptr;
		};
		return Recurse(m_Children);
	}

	bool FileNode::RemoveChild(FileNode& node)
	{
		auto it = m_Children.find(node.GetName());
		if (it != m_Children.end())
		{
			m_Children.erase(it);
			return true;
		}
		return false;
	}
	FileNode& FileNode::AddChild(std::unique_ptr<FileNode> node)
	{
		auto[it, inserted] = m_Children.insert_or_assign(node->GetName(), std::move(node));
		return *it->second;
	}

	BranchSharedLocker FileNode::LockBranchShared()
	{
		return BranchSharedLocker(*this);
	}
	BranchExclusiveLocker FileNode::LockBranchExclusive()
	{
		return BranchExclusiveLocker(*this);
	}
}
