# KxVirtualFileSystem (KxVFS)
A Dokany2-based user mode Virtual File System (VFS) implementation.

This library used in [Kortex Mod Manager](https://github.com/KerberX/Kortex-Mod-Manager) to provide dynamic merging of game and mods content without cluttering game folder.

# Building
- You will need [Dokany](https://github.com/dokan-dev/dokany) [2 Beta](https://github.com/dokan-dev/dokany/releases/tag/v2.0.0-BETA1) (`Corillian-asyncio` branch) and Visual Studio 2019.
- Forget that, since v2.1 you need to link against [my own fork](https://github.com/KerberX/dokany/tree/Corillian-asyncio) of the same branch.
- Go to project **Properties -> C/C++ -> General -> Additional Include Directories** and change include path. They should point to Dokany2 root folder and `sys` folder inside it.
- Go to **Linker -> General -> Additional Library Directories** and change path to Dokany2 lib file (`Win32\Release` and `x64\Release` folders).
- Build KxVFS! All configurations for x86 and x64.
