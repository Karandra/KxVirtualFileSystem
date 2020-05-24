#pragma once
#include "KxVirtualFileSystem/Common.hpp"
#include "KxVirtualFileSystem/Utility/EnumClassOperations.h"
#include "KxVirtualFileSystem/Utility.h"
#include "BranchLocker.h"

namespace KxVFS
{
	enum class PathParts
	{
		None = 0,
		Namespace = 1 << 0,
		BaseDirectory = 1 << 1,
		RelativePath = 1 << 2,
		Name = 1 << 3
	};
	KxVFS_AllowEnumBitwiseOp(PathParts);
}

namespace KxVFS
{
	class FileNode final
	{
		friend class BranchSharedLocker;
		friend class BranchExclusiveLocker;

		public:
			using Map = std::map<DynamicStringW, std::unique_ptr<FileNode>>;
			using RefVector = std::vector<FileNode*>;
			using CRefVector = std::vector<const FileNode*>;

			using TreeWalker = std::function<bool(const FileNode&)>;

		private:
			enum class NavigateTo
			{
				Any,
				File,
				Folder
			};
			
		private:
			static FileNode* NavigateToElement(FileNode& rootNode, DynamicStringRefW relativePath, NavigateTo type, FileNode*& lastScanned);
			
			template<class T> static T* FindRootNode(T* thisNode)
			{
				T* node = thisNode;
				while (node && !node->IsRootNode())
				{
					node = node->GetParent();
				}
				return node;
			}
			template<class TNode, class TFunctor> auto DoWalkToRoot(TNode*&& leafNode, TFunctor&& func) const
			{
				TNode* node = leafNode;
				while (node && !node->IsRootNode())
				{
					if (func(*node))
					{
						node = node->GetParent();
					}
					else
					{
						break;
					}
				}
				return node;
			}
			template<class TItems, class TFunctor> auto DoWalkChildren(TItems&& children, TFunctor&& func) const -> FileNode*
			{
				for (auto&& [name, item]: children)
				{
					if (!func(*item))
					{
						return item.get();
					}
				}
				return nullptr;
			}

		public:
			static bool IsRequestToRootNode(DynamicStringRefW relativePath);
			static size_t HashFileName(DynamicStringRefW name);

		private:
			Map m_Children;
			FileItem m_Item;
			DynamicStringW m_NameLC;
			DynamicStringW m_FullPath;
			DynamicStringW m_RelativePath;
			DynamicStringRefW m_VirtualDirectory;
			FileNode* m_Parent = nullptr;
			SRWLock m_Lock;

		private:
			void Init(FileNode* parent = nullptr)
			{
				if (m_Parent)
				{
					m_VirtualDirectory = m_Parent->m_VirtualDirectory;
				}
				UpdatePaths();
			}
			void SetParent(FileNode* parent)
			{
				m_Parent = parent;
			}
			void UpdatePaths();
			bool RenameThisNode(DynamicStringRefW newName);

			SRWLock& GetLock()
			{
				return m_Lock;
			}
			DynamicStringW ConstructPath(PathParts options) const;

		public:
			FileNode() = default;
			FileNode(const FileItem& item, FileNode* parent = nullptr)
				:m_Item(item), m_Parent(parent)
			{
				Init(parent);
			}
			FileNode(FileItem&& item, FileNode* parent = nullptr)
				:m_Item(std::move(item)), m_Parent(parent)
			{
				Init(parent);
			}
			FileNode(DynamicStringRefW fullPath, FileNode* parent = nullptr)
				:m_Item(fullPath), m_Parent(parent)
			{
				Init(parent);
			}
			~FileNode() = default;

		public:
			bool IsRootNode() const
			{
				return m_Parent == nullptr;
			}
			void CopyBasicAttributes(const FileNode& other)
			{
				m_VirtualDirectory = other.m_VirtualDirectory;
				UpdatePaths();
			}
			void UpdateFileTree(DynamicStringRefW searchPath, bool queryShortNames = false);
			void MakeNull();

			FileNode* NavigateToFolder(DynamicStringRefW relativePath)
			{
				FileNode* lastScanned = nullptr;
				return NavigateToElement(*this, relativePath, NavigateTo::Folder, lastScanned);
			}
			FileNode* NavigateToFolder(DynamicStringRefW relativePath, FileNode*& lastScanned)
			{
				return NavigateToElement(*this, relativePath, NavigateTo::Folder, lastScanned);
			}

			FileNode* NavigateToFile(DynamicStringRefW relativePath)
			{
				FileNode* lastScanned = nullptr;
				return NavigateToElement(*this, relativePath, NavigateTo::File, lastScanned);
			}
			FileNode* NavigateToFile(DynamicStringRefW relativePath, FileNode*& lastScanned)
			{
				return NavigateToElement(*this, relativePath, NavigateTo::File, lastScanned);
			}

			FileNode* NavigateToAny(DynamicStringRefW relativePath)
			{
				FileNode* lastScanned = nullptr;
				return NavigateToElement(*this, relativePath, NavigateTo::Any, lastScanned);
			}
			FileNode* NavigateToAny(DynamicStringRefW relativePath, FileNode*& lastScanned)
			{
				return NavigateToElement(*this, relativePath, NavigateTo::Any, lastScanned);
			}

			const FileNode* WalkTree(const TreeWalker& func) const;
			
			template<class TFunctor> const FileNode* WalkToRoot(TFunctor&& func) const
			{
				return DoWalkToRoot(this, func);
			}
			template<class TFunctor> FileNode* WalkToRoot(TFunctor&& func)
			{
				return DoWalkToRoot(this, func);
			}

			template<class TFunctor> const FileNode* WalkChildren(TFunctor&& func) const
			{
				return DoWalkChildren(m_Children, func);
			}
			template<class TFunctor> FileNode* WalkChildren(TFunctor&& func)
			{
				return DoWalkChildren(m_Children, func);
			}

			bool HasChildren() const
			{
				return !m_Children.empty();
			}
			size_t GetChildrenCount() const
			{
				return m_Children.size();
			}
			const Map& GetChildren() const
			{
				return m_Children;
			}
			void ClearChildren()
			{
				m_Children.clear();
			}
			
			void ReserveChildren(size_t capacity)
			{
				//m_Children.reserve(capacity);
			}
			bool RemoveChild(FileNode& node);
			void RemoveThisChild()
			{
				if (m_Parent)
				{
					m_Parent->RemoveChild(*this);
				}
			}
			FileNode& AddChild(std::unique_ptr<FileNode> node);
			FileNode& AddChild(std::unique_ptr<FileNode> node, DynamicStringRefW virtualDirectory)
			{
				FileNode& ref = AddChild(std::move(node));
				ref.SetVirtualDirectory(virtualDirectory);
				return ref;
			}

			bool HasParent() const
			{
				return m_Parent != nullptr;
			}
			const FileNode* GetParent() const
			{
				return m_Parent;
			}
			FileNode* GetParent()
			{
				return m_Parent;
			}
			
			const FileNode* GetRootNode() const
			{
				return FindRootNode(this);
			}
			FileNode* GetRootNode()
			{
				return FindRootNode(this);
			}

			DynamicStringRefW GetNameLC() const
			{
				return m_NameLC;
			}
			DynamicStringRefW GetName() const
			{
				return m_Item.GetName();
			}
			bool SetName(DynamicStringRefW name)
			{
				if (!HasParent() || RenameThisNode(name))
				{
					m_Item.SetName(name);
					UpdatePaths();
					return true;
				}
				return false;
			}
			DynamicStringW GetFileExtension() const
			{
				return m_Item.GetFileExtension();
			}

			DynamicStringRefW GetSource() const
			{
				return m_Item.GetSource();
			}
			DynamicStringW GetSourceWithNS() const
			{
				DynamicStringW path = Utility::GetLongPathPrefix();
				path += m_Item.GetSource();
				return path;
			}

			DynamicStringRefW GetFullPath() const
			{
				return m_FullPath;
			}
			DynamicStringW GetFullPathWithNS() const
			{
				DynamicStringW path = Utility::GetLongPathPrefix();
				path += m_FullPath;
				return path;
			}
			
			DynamicStringRefW GetRelativePath() const
			{
				if (!m_VirtualDirectory.empty())
				{
					return m_RelativePath;
				}
				return {};
			}
			DynamicStringRefW GetVirtualDirectory() const
			{
				return m_VirtualDirectory;
			}
			void SetVirtualDirectory(DynamicStringRefW path)
			{
				m_VirtualDirectory = path;
				UpdatePaths();
			}

			const FileItem& GetItem() const
			{
				return m_Item;
			}
			const FileItem& CopyItem(const FileNode& other)
			{
				m_Item = other.m_Item;
				return m_Item;
			}
			const FileItem& TakeItem(FileNode&& other)
			{
				m_Item = std::move(other.m_Item);
				return m_Item;
			}
			const FileItem& UpdateItemInfo(bool queryShortName = false)
			{
				m_Item.UpdateInfo(m_FullPath, queryShortName);
				return m_Item;
			}
			const FileItem& UpdateItemInfo(DynamicStringRefW fullPath, bool queryShortName = false)
			{
				m_Item.UpdateInfo(fullPath, queryShortName);
				return m_Item;
			}

			FileAttributes GetAttributes() const
			{
				return m_Item.GetAttributes();
			}
			void SetAttributes(FileAttributes attributes)
			{
				m_Item.SetAttributes(attributes);
			}

			bool IsReadOnly() const
			{
				return m_Item.IsReadOnly();
			}
			bool IsDirectory() const
			{
				return m_Item.IsDirectory();
			}
			bool IsFile() const
			{
				return m_Item.IsFile();
			}
			
			int64_t GetFileSize() const
			{
				return m_Item.GetFileSize();
			}
			void SetFileSize(int64_t fileSize)
			{
				m_Item.SetFileSize(fileSize);
			}

			FILETIME GetCreationTime() const
			{
				return m_Item.GetCreationTime();
			}
			template<class T> void SetCreationTime(T&& value)
			{
				m_Item.SetCreationTime(value);
			}

			FILETIME GetModificationTime() const
			{
				return m_Item.GetModificationTime();
			}
			template<class T> void SetModificationTime(T&& value)
			{
				m_Item.SetModificationTime(value);
			}

			FILETIME GetLastAccessTime() const
			{
				return m_Item.GetLastAccessTime();
			}
			template<class T> void SetLastAccessTime(T&& value)
			{
				m_Item.SetLastAccessTime(value);
			}

		public:
			void FromBY_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION& byHandleInfo)
			{
				m_Item.FromBY_HANDLE_FILE_INFORMATION(byHandleInfo);
			}
			void FromFILE_BASIC_INFORMATION(const Dokany2::FILE_BASIC_INFORMATION& basicInfo)
			{
				m_Item.FromFILE_BASIC_INFORMATION(basicInfo);
			}

		public:
			[[nodiscard]] MoveableSharedSRWLocker LockShared()
			{
				return MoveableSharedSRWLocker(m_Lock);
			}
			[[nodiscard]] MoveableExclusiveSRWLocker LockExclusive()
			{
				return MoveableExclusiveSRWLocker(m_Lock);
			}
			
			[[nodiscard]] BranchSharedLocker LockBranchShared();
			[[nodiscard]] BranchExclusiveLocker LockBranchExclusive();
		};
}
