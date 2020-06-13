#include "stdafx.h"
#include "KxVFS/Utility.h"
#include "FileNode.h"

namespace KxVFS
{
	FileNode* FileNode::NavigateToElement(FileNode& rootNode, DynamicStringRefW relativePath, NavigateTo type, FileNode*& lastScanned) noexcept
	{
		if ((type == NavigateTo::Folder || type == NavigateTo::Any) && IsRequestToRootNode(relativePath))
		{
			return &rootNode;
		}

		lastScanned = &rootNode;
		if (rootNode.HasChildren())
		{
			auto ScanChildren = [&lastScanned](FileNode& scannedNode, DynamicStringRefW folderName) -> FileNode*
			{
				auto it = scannedNode.m_Children.find(folderName);
				if (it != scannedNode.m_Children.end())
				{
					lastScanned = it->second.get();
					return lastScanned;
				}
				return nullptr;
			};
			auto StripQuotes = [](DynamicStringRefW folderName)
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
			DynamicStringW relativePathLC = Utility::StringToLower(relativePath);
			Utility::String::SplitBySeparator(relativePathLC, L'\\', [&ScanChildren, &StripQuotes, &finalNode, &rootNode](DynamicStringRefW folderName)
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

	bool FileNode::IsRequestToRootNode(DynamicStringRefW relativePath) noexcept
	{
		return relativePath.empty() || relativePath == L"\\" || relativePath == L"/";
	}
	size_t FileNode::HashFileName(DynamicStringRefW name) noexcept
	{
		return Utility::Comparator::StringHashNoCase()(name);
	}

	void FileNode::UpdatePaths()
	{
		if (!m_VirtualDirectory.empty())
		{
			m_Item.SetSource(ConstructPath(PathParts::BaseDirectory|PathParts::RelativePath));
			m_FullPath = ConstructPath(PathParts::BaseDirectory|PathParts::RelativePath|PathParts::Name);
			m_RelativePath = ConstructPath(PathParts::RelativePath|PathParts::Name);
		}
		else
		{
			m_Item.SetSource({});
			m_FullPath.clear();
			m_RelativePath.clear();
		}
		m_NameLC = Utility::StringToLower(m_Item.GetName());
	}
	bool FileNode::RenameThisNode(DynamicStringRefW newName)
	{
		// Rename this node in parent's children
		Map& parentItems = m_Parent->m_Children;

		// Node name must still be old name when this function is called
		if (auto nodeHandle = parentItems.extract(m_NameLC))
		{
			nodeHandle.key() = Utility::StringToLower(newName);
			parentItems.insert(std::move(nodeHandle));

			// If we succeed, the caller must change the name in 'm_Item' and call 'UpdatePaths'
			return true;
		}
		return false;
	}
	DynamicStringW FileNode::ConstructPath(FlagSet<PathParts> options) const
	{
		DynamicStringW fullPath = options & PathParts::Namespace ? Utility::GetLongPathPrefix() : NullDynamicStringW;
		if (options & PathParts::BaseDirectory)
		{
			fullPath += m_VirtualDirectory;
			fullPath += L'\\';
		}

		if (options & PathParts::RelativePath)
		{
			DynamicStringW relativePath;
			WalkToRoot([this, &relativePath](const FileNode& node)
			{
				if (&node != this)
				{
					DynamicStringW name = node.GetName();
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
	void FileNode::UpdateFileTree(DynamicStringRefW searchPath, bool queryShortNames)
	{
		m_VirtualDirectory = searchPath;
		UpdatePaths();
		ClearChildren();

		auto BuildTreeBranch = [this, queryShortNames](FileNode::RefVector& directories, DynamicStringRefW path, FileNode& treeNode, FileNode* parentNode)
		{
			FileFinder finder(path, L"*");
			finder.QueryShortNames(queryShortNames);
			for (FileItem item = finder.FindNext(); item.IsOK(); item = finder.FindNext())
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
	void FileNode::MakeNull() noexcept
	{
		ClearChildren();
		m_Item = {};
		m_NameLC.clear();
		m_VirtualDirectory = {};
		m_FullPath.clear();
		m_RelativePath.clear();
		m_Parent = nullptr;
	}

	const FileNode* FileNode::WalkTree(const TreeWalker& func) const
	{
		std::function<const FileNode* (const FileNode::Map&)> Recurse;
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

	bool FileNode::RemoveChild(FileNode& node) noexcept
	{
		auto it = m_Children.find(node.GetNameLC());
		if (it != m_Children.end())
		{
			m_Children.erase(it);
			return true;
		}
		return false;
	}
	FileNode& FileNode::AddChild(std::unique_ptr<FileNode> node)
	{
		DynamicStringW name = node->GetNameLC();
		auto [it, inserted] = m_Children.insert_or_assign(std::move(name), std::move(node));
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
