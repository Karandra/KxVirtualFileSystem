/*
Copyright © 2019 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#include "KxVirtualFileSystem/KxVirtualFileSystem.h"
#include "KxVirtualFileSystem/Utility.h"
#include "FileNode.h"

namespace
{
	struct FileNameHasher
	{
		// From Boost
		template<class T> static void hash_combine(size_t& seed, T v) noexcept
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
		}
		
		size_t operator()(KxVFS::KxDynamicStringRefW value) const noexcept
		{
			size_t hashValue = 0;
			for (wchar_t c: value)
			{
				hash_combine(hashValue, KxVFS::Utility::CharToLower(c));
			}
			return hashValue;
		}
	};
}

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
				const size_t hash = FileNameHasher()(folderName);
				for (const auto& node: scannedNode.GetChildren())
				{
					if (hash == node->GetNameHash())
					{
						lastScanned = node.get();
						return node.get();
					}
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

			if (finalNode == nullptr || (type == NavigateTo::Folder && !finalNode->IsDirectory()) || (type == NavigateTo::File && !finalNode->IsFile()))
			{
				return nullptr;
			}

			if (finalNode)
			{
				lastScanned = finalNode->GetParent();
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
		return FileNameHasher()(name);
	}

	KxDynamicStringW FileNode::ConstructPath(PathParts options) const
	{
		KxDynamicStringW fullPath = ToBool(options & PathParts::Namespace) ? Utility::GetLongPathPrefix() : KxNullDynamicStringW;
		if (ToBool(options & PathParts::BaseDirectory))
		{
			fullPath += m_VirtualDirectory;
			fullPath += L'\\';
		}

		if (ToBool(options & PathParts::RelativePath))
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

		if (ToBool(options & PathParts::Name))
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
					node.CalcNameHash();

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
		m_IndexWithinParent = std::numeric_limits<size_t>::max();
		m_NameHash = 0;
		m_IsChildrenSorted = true;
	}

	const FileNode* FileNode::WalkTree(const TreeWalker& func) const
	{
		std::function<const FileNode*(const FileNode::Vector&)> Recurse;
		Recurse = [&Recurse, &func](const FileNode::Vector& children) -> const FileNode*
		{
			for (const auto& node: children)
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

	void FileNode::SortChildren()
	{
		if (!m_IsChildrenSorted)
		{
			std::sort(m_Children.begin(), m_Children.end(), [](const auto& node1, const auto& node2)
			{
				return Utility::Comparator::IsLessNoCase(node1->GetName(), node2->GetName());
			});
			RecalcIndexes();
			m_IsChildrenSorted = true;
		}
	}
	bool FileNode::RemoveChild(FileNode& node)
	{
		const size_t index = node.GetIndexWithinParent();
		if (index < m_Children.size() && m_Children[index].get() == &node)
		{
			m_Children.erase(m_Children.begin() + index);
			RecalcIndexes(index);
			return true;
		}
		return false;
	}
	FileNode& FileNode::AddChild(std::unique_ptr<FileNode> node)
	{
		FileNode& ref = *m_Children.emplace_back(std::move(node));
		ref.SetIndexWithinParent(m_Children.size() - 1);
		ref.CalcNameHash();

		InvalidateSorting();
		return ref;
	}
	FileNode* FileNode::CreateDirectoryTree(KxDynamicStringRefW basePath, KxDynamicStringRefW branchPath)
	{
		KxDynamicStringW fullPath = Utility::GetLongPathPrefix();
		fullPath += basePath;

		FileNode* parentNode = this;
		Utility::String::SplitBySeparator(branchPath, L"\\", [&fullPath, &parentNode](KxDynamicStringRefW folderName)
		{
			fullPath += L'\\';
			fullPath += folderName;

			parentNode = &parentNode->AddChild(std::make_unique<FileNode>(fullPath.get_view(), parentNode));
			return true;
		});
		return parentNode != this ? parentNode : nullptr;
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
