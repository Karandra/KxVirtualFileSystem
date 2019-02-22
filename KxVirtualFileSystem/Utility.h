/*
Copyright © 2018 Kerber. All rights reserved.

You should have received a copy of the GNU LGPL v3
along with KxVirtualFileSystem. If not, see https://www.gnu.org/licenses/lgpl-3.0.html.
*/
#pragma once

// Common utilities
#include "Utility/Common.h"

// KxDynamicString
#include "Utility/KxDynamicString/KxDynamicString.h"

// Ported from KxFramework
#include "Utility/KxCallAtScopeExit.h"
#include "Utility/KxComparator.h"

#include "Utility/KxFileSystem/KxIFileFinder.h"
#include "Utility/KxFileSystem/KxFileFinder.h"
#include "Utility/KxFileSystem/KxFileItem.h"

// Own classes
#include "Utility/IContext.h"
#include "Utility/HandleWrapper.h"
#include "Utility/FileHandle.h"
#include "Utility/TokenHandle.h"
#include "Utility/SecurityObject.h"
#include "Utility/ServiceManager.h"
#include "Utility/CriticalSection.h"
#include "Utility/SRWLock.h"
#include "Utility/DispatcherMapBuilder.h"
