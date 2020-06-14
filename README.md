# KxVirtualFileSystem (KxVFS)
A Dokany2-based user mode Virtual File System (VFS) implementation.

This library used in [Kortex Mod Manager](https://github.com/KerberX/Kortex-Mod-Manager) to provide dynamic merging of game and mods content without cluttering game folder.

# Building
- ~~You will need [Dokany](https://github.com/dokan-dev/dokany) [2 Beta](https://github.com/dokan-dev/dokany/releases/tag/v2.0.0-BETA1) (`Corillian-asyncio` branch) and Visual Studio 2019.~~
- Forget that, since v2.1 you need to link against [my own fork](https://github.com/KerberX/dokany) of the `v2.x-develop` branch.
- Define `DOKAN_STATIC_LINK` macro for `dokan` project.
- Install `dokany-kerberx` package in **VCPkg**.
- Build KxVFS! All configurations for x86 and x64.

# As a dependency
Use **VCPkg** package manager with portfiles in `VCPkg/ports` folder to download and build latest revision of the **KxVFS**. Name in portfile: `kxvfs`.
