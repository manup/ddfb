#include <dlfcn.h>


void *U_LoadLibrary(const char *filename, int flags)
{
	flags = RTLD_LAZY;
	U_ASSERT(filename);
	return dlopen(filename, flags);
}

void U_CloseLibrary(U_Library *handle)
{
	U_ASSERT(handle);
	dlclose(handle);
}

void *U_GetLibrarySymbol(U_Library *handle, const char *symbol)
{
	U_ASSERT(handle);
	return dlsym(handle, symbol);
}
