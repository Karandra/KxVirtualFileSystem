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
