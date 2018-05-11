#define WIN32_NO_STATUS
#define NOMINMAX
#include <Windows.h>
#undef CreateService
#undef StartService
#undef GetFileAttributes
#undef FormatMessage
