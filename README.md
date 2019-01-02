# KxVirtualFileSystem (KxVFS)
A Dokany2-based user mode Virtual File System (VFS) implementation.

This library used in [Kortex Mod Manager](https://github.com/KerberX/Kortex-Mod-Manager) to provide dynamic merging of game and mods content without cluttering game folder.

# Building
- You will need [Dokany](https://github.com/dokan-dev/dokany) [2 Beta](https://github.com/dokan-dev/dokany/releases/tag/v2.0.0-BETA1) and Visual Studio 2017.
  - It is assumed that Dokany 2 Beta is built as static library so go to Dokany2 solution and change configuration to static library (**Properties -> General -> Configuration Type -> Static Library (.lib)**).
  - Dokany is not designed to be built as static library, but it's really easy to fix. Go to `dokan\dokan.c` file, find `DllMain` function and comment it out entirely. KxVFS has its own `DllMain` (albeit the same as Dokany's `DllMain`).
  - Rebuild both **Debug** and **Release** configuration for x86 and x64.
- Go to project **Properties -> C/C++ -> General -> Additional Include Directories** and change include path. They should point to Dokany2 root folder and `sys` folder inside it.
- Go to **Linker -> General -> Additional Library Directories** and change path to Dokany2 lib file (`Win32\Release` and `x64\Release` folders).
- Build KxVFS! Both **Debug** and **Release** configuration for x86 and x64.
