
static U_Arena _scratch_arena;

void U_ScratchInit(unsigned size)
{
    U_ASSERT(_scratch_arena.buf == NULL);
    U_InitArena(&_scratch_arena, size);
}

void *U_ScratchAlloc(unsigned size)
{
    void *p;
    U_ASSERT(_scratch_arena.buf != NULL);

    p = U_AllocArena(&_scratch_arena, size, U_ARENA_ALIGN_8);
    U_ASSERT(p && "scratch arena exhausted");
    return p;
}

unsigned U_ScratchPos(void)
{
    return _scratch_arena.size;
}
void U_ScratchRestore(unsigned pos)
{
    if (pos < _scratch_arena.size)
        _scratch_arena.size = pos;
}

void U_ScratchReset(void)
{
    _scratch_arena.size = 0;
}

void U_ScratchFree(void)
{
    U_FreeArena(&_scratch_arena);
}
