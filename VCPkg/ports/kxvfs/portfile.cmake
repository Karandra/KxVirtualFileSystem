# Common Ambient Variables:
#   CURRENT_BUILDTREES_DIR    = ${VCPKG_ROOT_DIR}\buildtrees\${PORT}
#   CURRENT_PACKAGES_DIR      = ${VCPKG_ROOT_DIR}\packages\${PORT}_${TARGET_TRIPLET}
#   CURRENT_PORT_DIR          = ${VCPKG_ROOT_DIR}\ports\${PORT}
#   CURRENT_INSTALLED_DIR     = ${VCPKG_ROOT_DIR}\installed\${TRIPLET}
#   DOWNLOADS                 = ${VCPKG_ROOT_DIR}\downloads
#   PORT                      = current port name (zlib, etc)
#   TARGET_TRIPLET            = current triplet (x86-windows, x64-windows-static, etc)
#   VCPKG_CRT_LINKAGE         = C runtime linkage type (static, dynamic)
#   VCPKG_LIBRARY_LINKAGE     = target library linkage type (static, dynamic)
#   VCPKG_ROOT_DIR            = <C:\path\to\current\vcpkg>
#   VCPKG_TARGET_ARCHITECTURE = target architecture (x64, x86, arm)
#   VCPKG_TOOLCHAIN           = ON OFF
#   TRIPLET_SYSTEM_ARCH       = arm x86 x64
#   BUILD_ARCH                = "Win32" "x64" "ARM"
#   MSBUILD_PLATFORM          = "Win32"/"x64"/${TRIPLET_SYSTEM_ARCH}
#   DEBUG_CONFIG              = "Debug Static" "Debug Dll"
#   RELEASE_CONFIG            = "Release Static"" "Release DLL"
#   VCPKG_TARGET_IS_WINDOWS
#   VCPKG_TARGET_IS_UWP
#   VCPKG_TARGET_IS_LINUX
#   VCPKG_TARGET_IS_OSX
#   VCPKG_TARGET_IS_FREEBSD
#   VCPKG_TARGET_IS_ANDROID
#   VCPKG_TARGET_IS_MINGW
#   VCPKG_TARGET_EXECUTABLE_SUFFIX
#   VCPKG_TARGET_STATIC_LIBRARY_SUFFIX
#   VCPKG_TARGET_SHARED_LIBRARY_SUFFIX
#
# 	See additional helpful variables in /docs/maintainers/vcpkg_common_definitions.md

# Specifies if the port install should fail immediately given a condition
# vcpkg_fail_port_install(MESSAGE "kxframework currently only supports Linux and Mac platforms" ON_TARGET "Windows")

include(vcpkg_common_functions)

if (NOT VCPKG_LIBRARY_LINKAGE STREQUAL static)
	message(FATAL_ERROR "Unsupported linkage: ${VCPKG_LIBRARY_LINKAGE}")
endif()

set(VCPKG_USE_HEAD_VERSION ON)
vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO KerberX/KxVirtualFileSystem
	REF master
	SHA512 1
	HEAD_REF master
)

# # Check if one or more features are a part of a package installation.
# # See /docs/maintainers/vcpkg_check_features.md for more details
# vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
#   FEATURES # <- Keyword FEATURES is required because INVERTED_FEATURES are being used
#		tbb		WITH_TBB
#   INVERTED_FEATURES
#		tbb		ROCKSDB_IGNORE_PACKAGE_TBB
# )

if (VCPKG_TARGET_ARCHITECTURE MATCHES "x86")
	set(BUILD_ARCH "Win32")
	set(OUTPUT_DIR "Win32")
elseif (VCPKG_TARGET_ARCHITECTURE MATCHES "x64")
	set(BUILD_ARCH "x64")
else()
	message(FATAL_ERROR "Unsupported architecture: ${VCPKG_TARGET_ARCHITECTURE}")
endif()

set(VcpkgTriplet ${TARGET_TRIPLET})
if (VCPKG_LIBRARY_LINKAGE STREQUAL static)
	vcpkg_install_msbuild(
		SOURCE_PATH ${SOURCE_PATH}
		PROJECT_SUBPATH KxVirtualFileSystem.vcxproj
		PLATFORM ${BUILD_ARCH}
		OPTIONS /p:OverrideVcpkgTriplet=true
		OPTIONS /p:OverrideVcpkgTripletName=${TARGET_TRIPLET}
		OPTIONS /p:ForceImportBeforeCppTargets=${CURRENT_PORT_DIR}/vcpkg.targets
		OPTIONS /p:VcpkgApplocalDeps=false
		)
endif()

# Copy headers
file(COPY ${SOURCE_PATH}/KxVFS DESTINATION ${CURRENT_PACKAGES_DIR}/include FILES_MATCHING PATTERN *.h)
file(COPY ${SOURCE_PATH}/KxVFS DESTINATION ${CURRENT_PACKAGES_DIR}/include FILES_MATCHING PATTERN *.hpp)

vcpkg_copy_pdbs()

# Copy license
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/kxvfs RENAME copyright)
