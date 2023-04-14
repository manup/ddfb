#ifndef U_ARENA_H
#define U_ARENA_H

#define U_ARENA_ALIGN_1  1
#define U_ARENA_ALIGN_8  8
#define U_ARENA_INVALID_PTR 0xFFFFFFFFU
#define U_ARENA_SIZE_MASK 0x7FFFFFFFU
#define U_ARENA_STATIC_MEM_FLAG 0x80000000U

typedef struct U_Arena
{
	void *buf;
	u32 size;
	/* STATIC_MEM_FLAG is set in _total_size when the memory is not
	   owned by the arena.
	 */
	u32 _total_size;
} U_Arena;

#ifdef U_STATIC_ASSERT
  U_STATIC_ASSERT((sizeof(U_Arena) == 16), "U_Arena unexpected size");
#endif

void U_InitArena(U_Arena *arena, unsigned size);
void *U_AllocArena(U_Arena *arena, unsigned size, unsigned alignment);
void U_FreeArena(U_Arena *arena);
u32 U_GetArenaPtr(U_Arena *arena, void *mem);
void *U_GetArenaMem(U_Arena *arena, u32 ptr);

#endif /* U_ARENA_H */
