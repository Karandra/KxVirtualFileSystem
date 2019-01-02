/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

#define WIN32_NO_STATUS
#define NOMINMAX

#include <Windows.h>
#undef CreateService
#undef StartService
#undef GetFileAttributes
#undef FormatMessage
