// No include guard, we need to include this file multiple times
#include <utility>

// Macro to make quick template function to resolve A/W functions
#ifndef Kx_MakeWinUnicodeCallWrapper
#ifdef UNICODE
#define Kx_MakeWinUnicodeCallWrapper(funcName)	\
			template<class... Args>	\
			auto funcName(Args&&... arg)	\
			{	\
				return ::funcName##W(std::forward<Args>(arg)...);	\
			}
#else
#define Kx_MakeWinUnicodeCallWrapper(funcName)	\
					template<class... Args>	\
					auto funcName(Args&&... arg)	\
					{	\
						return ::funcName##A(std::forward<Args>(arg)...);	\
					}
#endif
#endif

// Undefine Unicode wrapper macros
#undef min
#undef max
#undef GetObject
#undef SetCurrentDirectory
#undef CopyFile
#undef MoveFile
#undef DeleteFile
#undef GetBinaryType
#undef MessageBox
#undef GetFreeSpace
#undef PlaySound
#undef RegisterClass
#undef CreateEvent
#undef GetFirstChild
#undef GetNextSibling
#undef GetPrevSibling
#undef GetWindowStyle
#undef GetShortPathName
#undef GetLongPathName
#undef GetFullPathName
#undef GetFileAttributes
#undef EnumResourceTypes
#undef LoadImage
#undef UpdateResource
#undef BeginUpdateResource
#undef EndUpdateResource
#undef EnumResourceLanguages
#undef FormatMessage
#undef GetCommandLine
#undef CreateProcess
#undef GetUserName
#undef FindFirstFile
#undef FindNextFile
#undef GetEnvironmentVariable
#undef SetEnvironmentVariable
#undef ExitWindows
#undef GetCompressedFileSize
#undef GetTempPath
#undef CreateDirectory
#undef CompareString
#undef EnumDisplayDevices
#undef ExpandEnvironmentStrings
#undef EnumResourceNames
#undef GetMessage
#undef OpenService
#undef CreateService
#undef WriteConsole
#undef CreateProcess
#undef CreateDirectory

#ifdef ZeroMemory
#undef ZeroMemory
inline void* ZeroMemory(void* ptr, size_t size) noexcept
{
	return std::memset(ptr, 0, size);
}
#endif

#ifdef CopyMemory
#undef CopyMemory
inline void* CopyMemory(void* dst, const void* src, size_t size) noexcept
{
	return std::memcpy(dst, src, size);
}
#endif

#ifdef MoveMemory
#undef MoveMemory
inline void* MoveMemory(void* dst, const void* src, size_t size) noexcept
{
	return std::memmove(dst, src, size);
}
#endif

#ifdef FillMemory
#undef FillMemory
inline void* FillMemory(void* dst, size_t length, int fill) noexcept
{
	return std::memset(dst, fill, length);
}
#endif

#ifdef EqualMemory
#undef EqualMemory
inline bool EqualMemory(const void* left, const void* right, size_t size) noexcept
{
	return std::memcmp(left, right, size) == 0;
}
#endif

#ifdef SecureZeroMemory
#undef SecureZeroMemory
inline void* SecureZeroMemory(void* ptr, size_t size) noexcept
{
	return ::RtlSecureZeroMemory(ptr, size);
}
#endif
