/* #define DEBUG_MEMLIST */

#define U_MAX_MEM_TRACKS 8

#define U_MIN_MEM_ALLOC_SIZE 32
#define U_MAX_MEM_ALLOC_SIZE 0x4000000 /* 67 MB */

#define U_MEM_ALLOC_SIZE_0   U_MIN_MEM_ALLOC_SIZE
#define U_MEM_ALLOC_SIZE_1   64
#define U_MEM_ALLOC_SIZE_2   128
#define U_MEM_ALLOC_SIZE_3   256
#define U_MEM_ALLOC_SIZE_4   512
#define U_MEM_ALLOC_SIZE_5   1024
#define U_MEM_ALLOC_SIZE_6   4096
#define U_MEM_ALLOC_SIZE_7   U_MAX_MEM_ALLOC_SIZE     /* as large as needed */

typedef struct u_mem u_mem;
typedef struct u_mem_track u_mem_track;

struct u_mem
{
    struct u_mem *next;
    unsigned size;
    int reused;
    U_AllocType type;
    const char *src_function;
    int src_line;
    double free_time;
    unsigned char data[4];
};

struct u_mem_track
{
    unsigned size;
    unsigned size_allocated;
    u_mem *alloc_list;
    u_mem *free_list;
};

static u_mem *_u_mem = NULL; /* all memory allocations */
static u_mem_track _u_mem_tracks[U_MAX_MEM_TRACKS];

static void _u_mem_list_add(u_mem **list, u_mem *mem);
static int _u_mem_list_remove(u_mem **list, u_mem *mem);

void U_MemoryInit()
{
    U_ASSERT(_u_mem == NULL);
    U_ASSERT(_u_mem_tracks[0].size == 0);

    U_bzero(&_u_mem_tracks[0], sizeof(_u_mem_tracks));

    _u_mem_tracks[0].size = U_MEM_ALLOC_SIZE_0;
    _u_mem_tracks[1].size = U_MEM_ALLOC_SIZE_1;
    _u_mem_tracks[2].size = U_MEM_ALLOC_SIZE_2;
    _u_mem_tracks[3].size = U_MEM_ALLOC_SIZE_3;
    _u_mem_tracks[4].size = U_MEM_ALLOC_SIZE_4;
    _u_mem_tracks[5].size = U_MEM_ALLOC_SIZE_5;
    _u_mem_tracks[6].size = U_MEM_ALLOC_SIZE_6;
    _u_mem_tracks[7].size = U_MEM_ALLOC_SIZE_7;
}

void U_MemoryGarbageCollect()
{
    u_mem *mem;
    u_mem_track *track;
    unsigned i;
    double now;

    now = PL_GetTime();

    U_Printf("%s\n", U_FUNCTION);

    for (i = 0; i < U_ARRAY_SIZE(_u_mem_tracks); i++)
    {
        track = &_u_mem_tracks[i];

        mem = track->free_list;
        while (mem)
        {
            if (now - mem->free_time > 10.0)
            {
                _u_mem_list_remove(&track->free_list, mem);
                track->size_allocated -= mem->size;
                U_Printf("%s tracked[%u] size: %u, total %u\n", U_FUNCTION, i, track->size, track->size_allocated);
                free(mem);
                break;
            }
            else
            {
                mem = mem->next;
            }
        }
    }
}

void U_MemoryFree()
{
    u_mem *m;
    u_mem_track *track;
    unsigned i;

    for (i = 0; i < U_ARRAY_SIZE(_u_mem_tracks); i++)
    {
        track = &_u_mem_tracks[i];
        /*U_Printf("%s tracked[%u] size: %u, free %u\n", U_FUNCTION, i, track->size, track->size_allocated);*/

        while (track->free_list)
        {
            m = track->free_list;
            track->free_list = m->next;
            free(m);
        }

        while (track->alloc_list)
        {
            m = track->alloc_list;
            track->alloc_list = m->next;
            free(m);
        }
    }

}

#ifdef DEBUG_MEMLIST
static void _u_mem_list_print(u_mem **list)
{
    u_mem *mem;

    mem = *list;

    U_Printf("mem list %p: [ ", list);
    while (mem)
    {
        U_Printf("%p", mem);
        if (mem->next)
        {
            U_Printf(", ");
        }
        mem = mem->next;
    }
    U_Printf(" ]\n");
}
#endif

static void _u_mem_list_add(u_mem **list, u_mem *mem)
{
    u_mem *m;

    mem->next = NULL;

    if (*list == NULL)
    {
        *list = mem;
    }
    else
    {
        m = *list;
        while (m->next)
            m = m->next;

        U_ASSERT(!m->next);
        m->next = mem;
    }

#ifdef DEBUG_MEMLIST
    U_Printf("add mem %p to list %p\n", mem, list);
    _u_mem_list_print(list);
#endif
}

static int _u_mem_list_remove(u_mem **list, u_mem *mem)
{
    u_mem *m0;
    u_mem *m1;

    U_ASSERT(*list);
    U_ASSERT(mem);

    m0 = *list;
    m1 = *list;

    while (m1)
    {
        if (m1 == mem)
        {
            /* U_Printf("remove mem %p from list %p\n", mem, list); */
            if (m1 == *list) /* m1 is first element */
                *list = m1->next;
            else
                m0->next = m1->next;
            break;
        }

        m0 = m1;
        m1 = m1->next;
    }

#ifdef DEBUG_MEMLIST
    _u_mem_list_print(list);
#endif

    if (!m1)
    {
        U_ASSERT(0 && "failed removing a memory list element");
        return 0;
    }

    return 1;
}

static u_mem *_get_free_mem_for_size(u_mem_track *track, unsigned size)
{
    u_mem *mem;

    mem = track->free_list;

    while (mem)
    {
        if (mem->size >= size && (mem->size - size) < 128)
            break;
        mem = mem->next;
    }

    return mem;
}

void *U_AllocTracked(unsigned size, U_AllocType type, const char *src_function, int src_line)
{
    unsigned i;
    unsigned sz;
    u_mem *mem;
    u_mem_track *track;

    /* round up power of two */
    sz = U_MIN_MEM_ALLOC_SIZE;
    while (sz < size)
        sz *= 2;

    for (i = 0; i < U_MAX_MEM_TRACKS; i++)
    {
        if (_u_mem_tracks[i].size >= sz)
            break;
    }

    if (i == U_MAX_MEM_TRACKS)
    {
        U_ASSERT(0 && "request to allocate too much memory");
        return NULL;
    }

    track = &_u_mem_tracks[i];
    mem = _get_free_mem_for_size(track, size);

    if (mem)
    {
        if (mem->reused < 65536)
            mem->reused++;

        if (_u_mem_list_remove(&track->free_list, mem) == 0)
        {
            U_ASSERT(0 && "failed to remove from track->free_list, should not happen");
        }
    }
    else
    {
        mem = (u_mem*)malloc(sizeof(*mem) + sz);
        mem->next = NULL;
        mem->size = sz;
        mem->reused = 0;
        mem->src_function = U_FUNCTION;
        mem->src_line = __LINE__;
        mem->type = U_ALLOC_MANAGED;
        track->size_allocated += sz;
    }

    /* U_Printf("%s [%u] reused: %u, size: %u, type: %d, in %s():%d, total_allocated: %u\n", U_FUNCTION, i, mem->reused, mem->size, type, src_function, src_line, track->size_allocated); */

    U_ASSERT(mem);
    _u_mem_list_add(&track->alloc_list, mem);
    U_bzero(&mem->data[0], mem->size);
    mem->src_function = src_function;
    mem->src_line = src_line;
    mem->type = type;

    return &mem->data[0];
}

int U_FreeTracked(void *p)
{
    unsigned i;
    unsigned char *data;
    u_mem *mem;
    u_mem_track *track;

    U_ASSERT(p);
    data = p;
    mem = (u_mem*)(data - U_offsetof(u_mem, data));

    for (i = 0; i < U_MAX_MEM_TRACKS; i++)
    {
        if (_u_mem_tracks[i].size >= mem->size)
            break;
    }

    if (i == U_MAX_MEM_TRACKS)
    {
        U_ASSERT(0 && "request to free non tracked memory");
        return 0;
    }

    track = &_u_mem_tracks[i];

    if (_u_mem_list_remove(&track->alloc_list, mem) == 0)
        return 0;

    mem->free_time = PL_GetTime();
    _u_mem_list_add(&track->free_list, mem);

    /* U_Printf("%s track[%u] size: %u, reused: %d, type: %d, from %s():%d\n", U_FUNCTION, i, mem->size, mem->reused, mem->type, mem->src_function, mem->src_line); */
    return 1;
}

void *U_memalign(void *p, unsigned align)
{
    uintptr_t num;
    uintptr_t algn;
    void *p1;

    U_ASSERT(align == 1 || align == 8 || align == 16 || align == 32 || align == 64);

    num = (uintptr_t)p;
    algn = align;
    p1 = (void*)((num + (algn - 1)) & ~(algn - 1));

    U_ASSERT(p <= p1);
    return p1;
}

void *U_calloc(unsigned size)
{
    u_mem *p;

    p = (u_mem*)malloc(sizeof(*p) + size);
    p->next = NULL;
    p->size = size;
    p->reused = 0;
    p->src_function = U_FUNCTION;
    p->src_line = __LINE__;
    p->type = U_ALLOC_MANAGED;
    U_bzero(&p->data[0], size);

    U_Printf("calloc: %u bytes @ %p\n", size, (void*)p);

    _u_mem_list_add(&_u_mem, p);

    return (void*)&p->data[0];
}

void U_free(void *p)
{
    u_mem *mem;
    unsigned char *data;

    U_ASSERT(p);
    if (!p)
        return;

    data = p;
    mem = (u_mem*)(data - U_offsetof(u_mem, data));

    if (_u_mem_list_remove(&_u_mem, mem))
    {
        U_Printf("free: %u bytes @ %p\n", mem->size, (void*)mem);
    }
    else
    {
        U_ASSERT(0 && "tried to free unknown memory");
    }
}
