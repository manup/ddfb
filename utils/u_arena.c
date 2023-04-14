
void U_InitArena(U_Arena *arena, unsigned size)
{
    U_ASSERT((size & U_ARENA_SIZE_MASK) == size);
    arena->size = 0;
    arena->_total_size = size;
    arena->buf = U_AllocManaged(size);
}

void *U_AllocArena(U_Arena *arena, unsigned size, unsigned alignment)
{
    u8 *p;
    u8 *end;

    U_ASSERT(arena->buf);

    p = arena->buf;
    p += arena->size;
    p = U_memalign(p, alignment);

    end = arena->buf;
    end += (arena->_total_size & U_ARENA_SIZE_MASK);

    if ((end - p) > size)
    {
        arena->size = (unsigned)(p - (u8*)arena->buf);
        arena->size += size;
        return p;
    }

    U_ASSERT(0 && "U_AllocArena() mem exhausted");

    return NULL;
}

void U_FreeArena(U_Arena *arena)
{
    if ((arena->_total_size & U_ARENA_STATIC_MEM_FLAG) == 0)
        U_FreeTracked(arena->buf);

    U_bzero(arena, sizeof(*arena));
}

void *U_GetArenaMem(U_Arena *arena, u32 ptr)
{
    U_ASSERT(arena->buf);
    U_ASSERT(arena->size > ptr);

    if (arena->buf && arena->size > ptr)
        return (u8*)arena->buf + ptr;

    return NULL;
}

u32 U_GetArenaPtr(U_Arena *arena, void *mem)
{
    u8 *beg;
    u8 *end;
    u8 *m;

    U_ASSERT(arena->buf);

    m = mem;
    beg = arena->buf;
    end = beg + arena->size;


    U_ASSERT(m >= beg && m < end);
    if (arena && m >= beg && m < end)
        return (u32)(m - beg);

    return U_ARENA_INVALID_PTR;
}
