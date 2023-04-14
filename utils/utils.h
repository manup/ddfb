#ifndef CU_UTILS_H
#define CU_UTILS_H

#define U_UNUSED(x) (void)x

#define U_offsetof(st, m) \
    ((size_t)&(((st *)0)->m))

#define U_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/* dynamic buffer */
typedef struct U_buffer
{
	unsigned size;
	unsigned char *buf;
} U_buffer;

void U_BufferInit(U_buffer *b, unsigned size);
void U_BufferResize(U_buffer *b, unsigned size);
void U_BufferFree(U_buffer *b);

/* time */

double PL_GetTime();
void U_sleepms(int ms);

/* A time value holding milliseconds since epoch. */
#ifdef _U_HAS_S64_TYPE
  typedef i64 U_time;
#else
  typedef i32 U_time;
#endif

U_time U_TimeNow(void);
int U_TimeToISO8601(U_time time, void *buf, unsigned len);
U_time U_TimeFromISO8601(const char *str, unsigned len);

void *U_memmove(void *dst, const void *src, unsigned n);
void *U_memcpy(void *dst, const void *src, unsigned n);
int U_memcmp(const void *s1, const void *s2, unsigned n);
void U_bzero(void *dst, unsigned size);

/* files */

const char *PL_ConfigDir(void);
int PL_GetConfigFilePath(U_SStream *ss, const char *fname);

typedef struct PL_Stat
{
    unsigned long size;
    U_time mtime;
} PL_Stat;

int PL_FillRandom(unsigned char *data, unsigned int size);
int PL_RealPath(const char *path, char *resolved, unsigned bufsize);
int PL_LoadFile(const char *path, void *buf, unsigned bufsize);
int PL_WriteFile(const char *path, const void *buf, unsigned bufsize);
int PL_FileExists(const char *path);
int PL_MoveFile(const char *src, const char *dst);
int PL_DeleteFile(const char *path);
int PL_MakeDirectory(const char *path);
int PL_StatFile(const char *path, PL_Stat *st);

int U_LoadImage(const char *path, u8 *mem, unsigned memsize, int *x, int *y, int *comp);
void U_WriteImagePPM(const char *path, const u8 *data, unsigned channels, unsigned width, unsigned height);

void U_Printf(const char *format, ...);
void U_Write(const char *str, unsigned len);


/* random numbers */
float U_randf();
void U_rand32_seed(unsigned seed);
unsigned U_rand32();

unsigned U_strlen(const char *str);
const char *U_utf8_codepoint(const char *text, unsigned *codepoint);
int U_unicode_to_utf8(unsigned codepoint, unsigned char buf[5]);
unsigned long U_hash_djb2(const void *data, unsigned size);

/* sorting */
void U_qsort(void *base, unsigned long nmemb, unsigned long size,
             int (*compar)(const void *, const void *));

/* extensions to linmath.h */
#ifdef LINMATH_H

float vec2_dot(const vec2 a, const vec2 b);
void vec2_scalar_project(vec2 r, const vec2 p, const vec2 a, const vec2 b);
float vec2_angle(const vec2 a, const vec2 b);
void vec2_proj(vec2 r, const vec2 a, const vec2 b);
void vec2_perpendicular(vec2 r, const vec2 v);
float vec2_signed_area(const vec2 p1, const vec2 p2, const vec2 p3);

void CartesianToPolar(vec2 pol, const vec2 cartesian);
void PolarToCartesian(vec2 cartesian, const vec2 pol);

#endif /* LINMATH_H */

/* shared library loading */

typedef void U_Library;

void *U_LoadLibrary(const char *filename, int flags);
void U_CloseLibrary(U_Library *handle);
void *U_GetLibrarySymbol(U_Library *handle, const char *symbol);

#endif /* CU_UTILS_H */
