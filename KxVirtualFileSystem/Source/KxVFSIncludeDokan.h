/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/

#pragma warning(disable: 4005) // Macro redefinition

extern "C"
{
	#pragma warning(push)
	#pragma warning(disable: 4505) // Unreferenced local function has been removed

	// Define _EXPORTING to link statically
	#define _EXPORTING
	#include "Dokan/dokan.h"
	#include "Dokan/fileinfo.h"
	#undef _EXPORTING

	#pragma warning(pop)
}
