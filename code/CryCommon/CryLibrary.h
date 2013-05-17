#ifndef CRYLIBRARY_H__
#define CRYLIBRARY_H__

/*!
	CryLibrary
	
	Convenience-Macros which abstract the use of DLLs/shared libraries in a platform independent way.
	A short explanation of the different macros follows:
	
	CrySharedLibrarySupported:
		This macro can be used to test if the current active platform supports shared library calls. The default
		value is false. This gets redefined if a certain platform (WIN32 or LINUX) is desired.

	CrySharedLibraryExtension:
		The default extension which will get appended to library names in calls to CryLoadLibraryDefExt
		(see below).

	CryLoadLibrary(libName):
		Loads a shared library.

	CryLoadLibraryDefExt(libName):
		Loads a shared library. The platform-specific default extension is appended to the libName. This allows
		writing of somewhat platform-independent library loading code and is therefore the function which should
		be used most of the time, unless some special extensions are used (e.g. for plugins).
	
	CryGetProcAddress(libHandle, procName):
		Import function from the library presented by libHandle.
		
	CryFreeLibrary(libHandle):
		Unload the library presented by libHandle.
	
	HISTORY:
		03.03.2004 MarcoK
			- initial version
			- added to CryPlatform
*/

#include <stdio.h>

#if defined(WIN32) || defined(XENON)  || defined(GRINGO)
#ifdef WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#define CryLoadLibrary(libName) ::LoadLibrary(libName)








#endif //WIN32
	#define CrySharedLibraySupported true
	#define CrySharedLibrayExtension ".dll"
	#define CryGetProcAddress(libHandle, procName) ::GetProcAddress((HMODULE)(libHandle), procName)
	#define CryFreeLibrary(libHandle) ::FreeLibrary((HMODULE)(libHandle))
#elif (defined(LINUX) && !defined(_LIB))
	#include <dlfcn.h>
	#include <stdlib.h>
	#include "platform.h"

	// for compatibility with code written for windows
	#define CrySharedLibraySupported true
	#define CrySharedLibrayExtension ".so"
	#define CryGetProcAddress(libHandle, procName) ::dlsym(libHandle, procName)
	#define CryFreeLibrary(libHandle) ::dlclose(libHandle)

	#define HMODULE void*
	static const char* gEnvName("MODULE_PATH");

	static const char* GetModulePath()
	{
		return getenv(gEnvName);
	}

	static void SetModulePath(const char* pModulePath)
	{
		setenv(gEnvName, pModulePath?pModulePath:"",true);
	}

	static HMODULE CryLoadLibrary(const char* libName, const bool cAppend = true, const bool cLoadLazy = false)
	{
		//[K01]: linux version
		char path[_MAX_PATH];
		sprintf(path, "%s/%s", GetModulePath(), libName);
		return ::dlopen(path, RTLD_LAZY);
	}


























































#else



#define CrySharedLibraySupported false
#define CrySharedLibrayExtension ""
#define CryLoadLibrary(libName) NULL
#define CryLoadLibraryDefExt(libName) CryLoadLibrary(libName CrySharedLibrayExtension)
#define CryGetProcAddress(libHandle, procName) NULL
#define CryFreeLibrary(libHandle)
#endif 

#endif //CRYLIBRARY_H__
