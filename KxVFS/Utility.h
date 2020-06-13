#pragma once

// Common utilities
#include "Utility/Common.h"
#include "Utility/FlagSet.h"
#include "Utility/Win32Constants.h"
#include "Utility/WinKernelConstants.h"
#include "Utility/NtStatusConstants.h"

// KxDynamicString
#include "Utility/DynamicString/DynamicString.h"

// Ported from KxFramework
#include "Utility/CallAtScopeExit.h"
#include "Utility/Comparator.h"
#include "Utility/String.h"

#include "Utility/FileSystem/IFileFinder.h"
#include "Utility/FileSystem/FileFinder.h"
#include "Utility/FileSystem/FileItem.h"

// Own classes
#include "Utility/HandleWrapper.h"
#include "Utility/GenericHandle.h"
#include "Utility/FileHandle.h"
#include "Utility/TokenHandle.h"
#include "Utility/SearchHandle.h"
#include "Utility/SecurityObject.h"
#include "Utility/ServiceManager.h"
#include "Utility/ServiceHandle.h"
#include "Utility/ProcessHandle.h"
#include "Utility/CriticalSection.h"
#include "Utility/SRWLock.h"
