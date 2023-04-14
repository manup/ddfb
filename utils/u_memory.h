#ifndef U_MEMORY_H
#define U_MEMORY_H

#define U_KILO_BYTES(n) ((n) * 1000)
#define U_MEGA_BYTES(n) ((n) * 1000 * 1000)

/* memory */
typedef enum U_AllocType
{
    U_ALLOC_MANAGED,        /* caller is expected to release memory */
    U_ALLOC_FUNCTION_SCOPE, /* memory only valid for function scope */
    U_ALLOC_LOOP_SCOPE      /* memory only valid until next main loop run */
} U_AllocType;

#define U_AllocManaged(size) U_AllocTracked(size, U_ALLOC_MANAGED, U_FUNCTION, __LINE__)
#define U_AllocFunctionScope(size) U_AllocTracked(size, U_ALLOC_FUNCTION_SCOPE, U_FUNCTION, __LINE__)

void U_MemoryInit();
void U_MemoryGarbageCollect();
void U_MemoryFree();

/* Pool allocator */
/* Allocate tracked memory */
void *U_AllocTracked(unsigned size, U_AllocType type, const char *src_function, int src_line);
int U_FreeTracked(void *p);

void *U_memalign(void *p, unsigned align);
void *U_calloc(unsigned size);
void U_free(void *p);

#endif /* U_MEMORY_H */
