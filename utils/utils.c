
#ifdef _WIN32
  #include "pl_windows.c"
#endif

#ifdef USE_SDL
  #include "pl_sdl.c"
#endif

#ifdef PL_POSIX
  #include "pl_posix.c"
#endif

void U_BufferInit(U_buffer *b, unsigned size)
{
    b->size = 0;
    b->buf = NULL;

    if (size)
    {
        b->buf = (unsigned char*)U_AllocManaged(size);
        b->size = size;
    }
}

void U_BufferResize(U_buffer *b, unsigned size)
{
    unsigned to_copy;
    unsigned char *buf2;

    if (b->buf)
    {
        buf2 = (unsigned char*)U_AllocManaged(size);
        to_copy = U_min(b->size, size);
        U_memcpy(buf2, b->buf, to_copy);
        U_FreeTracked(b->buf);
        b->buf = buf2;
        b->size = size;
    }
    else
    {
        U_BufferInit(b, size);
    }
}

void U_BufferFree(U_buffer *b)
{
    U_FreeTracked(b->buf);
    b->size = 0;
    b->buf = NULL;
}

static unsigned _u_rand;

float U_randf()
{
    unsigned a;
    const unsigned max = ~0;

    a = _u_rand;

    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);

    _u_rand = a;

    return (float)a / (float)max;
}

void U_rand32_seed(unsigned seed)
{
    _u_rand = seed;
}

/*  LCG generator (BSD)
    https://tfetimes.com/c-linear-congruential-generator/
*/
unsigned U_rand32()
{
    const unsigned a = 1103515245;
    const unsigned c = 12345;
    const unsigned m = 2147483648;
    _u_rand = (a * _u_rand + c) % m;
    return _u_rand;
}

void U_Printf(const char *format, ...)
{
    va_list args;
    va_start (args, format);
#if defined USE_SDL && defined PL_MOBILE
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, format, args);
#else
    vprintf(format, args);
#endif
    va_end (args);
}

void U_Write(const char *str, unsigned len)
{
#ifdef PL_POSIX
    write(STDOUT_FILENO, str, len);
#endif
}

void *U_memmove(void *dst, const void *src, unsigned n)
{
    return memmove(dst, src, n);
}

void *U_memcpy(void *dst, const void *src, unsigned n)
{
    return memcpy(dst, src, n);
}

int U_memcmp(const void *s1, const void *s2, unsigned n)
{
    return memcmp(s1, s2, n);
}

void U_bzero(void *dst, unsigned size)
{
    if (size)
        memset(dst, 0, size);
}

unsigned U_strlen(const char *str)
{
    if (str) return (unsigned)strlen(str);
    return 0;
}

/* Convert utf-8 to unicode code point.

   Returns pointer to remainder of text. 'codepoint' is a valid codepoint
   or set to 0 for invalid utf8.

 */
const char *U_utf8_codepoint(const char *text, unsigned *codepoint)
{
    unsigned cp;

    cp = (unsigned)*text & 0xFF;
    text++;

    if ((cp & 0x80) == 0)
    {
        /* 1-byte ASCII */
    }
    else if ((cp & 0xE0) == 0xC0 && text[0] != 0) /*  110 prefix 2-byte char */
    {
        /* 110x xxxx 10xx xxxx */
        cp &= 0x1F;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
    }
    else if ((cp & 0xF0) == 0xE0 && text[0] != 0 && text[1] != 0) /*  1110 prefix 3-byte char */
    {
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        cp &= 0x0F;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
    }
    else if ((cp & 0xF8) == 0xF0 && text[0] != 0 && text[1] != 0 && text[2] != 0) /*  11110 prefix 4-byte char */
    {
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        cp &= 0x07;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
        cp <<= 6;
        cp |= (unsigned)*text & 0x3F;
        text++;
    }
    else
    {
        cp = 0; /* invalid */
    }

    *codepoint = cp;

    return text;
}

/* The buffer must be of size 5, the string gets zero terminated.
*/
int U_unicode_to_utf8(unsigned codepoint, unsigned char buf[5])
{
    if (codepoint <= 0x7F)
    {
        /* 1-byte ASCII */
        buf[0] = (char)codepoint;
        buf[1] = '\0';
        return 1;
    }

    if (codepoint > 0x10FFFF)
        codepoint = 0xFFFD; /* codepoint replacement */

    if (codepoint >= 0x80 && codepoint <= 0x7FF) /*  110 prefix 2-byte char */
    {
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xC0 | (codepoint & 0x1F);
        buf[2] = '\0';
        return 2;
    }
    else if (codepoint >= 0x800 && codepoint <= 0xFFFF) /*  1110 prefix 3-byte char */
    {
        /* 1110xxxx 10xxxxxx 10xxxxxx */
        buf[2] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xE0 | (codepoint & 0x0F);
        buf[3] = '\0';
        return 3;
    }
    else if (codepoint >= 0x10000 && codepoint <= 0x10FFFF) /*  11110 prefix 4-byte char */
    {
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        buf[3] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[2] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[1] = 0x80 | (codepoint & 0x3F);
        codepoint >>= 6;
        buf[0] = 0xF0 | (codepoint & 0x07);
        buf[4] = '\0';
        return 4;
    }

    return 0;
}

unsigned long U_hash_djb2(const void *data, unsigned size)
{
    int c;
    unsigned long hash;
    const unsigned char *str;

    hash = 5381;
    str = data;

    while (size)
    {
        size--;
        c = *str++;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

void U_qsort(void *base, unsigned long nmemb, unsigned long size,
             int (*compar)(const void *, const void *))
{
    qsort(base, (size_t)nmemb, (size_t)size, compar);
}

void U_sleepms(int ms)
{
#ifdef PL_WIN32
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* linmath extensions */
#ifdef LINMATH_H

float vec2_dot(const vec2 a, const vec2 b)
{
    return (a[0] * b[0]) + (a[1] * b[1]);
}

/* Scalar project ap onto ab.

         p
     ../
    /
   a..... n ......b
*/
void vec2_scalar_project(vec2 n, const vec2 p, const vec2 a, const vec2 b)
{
    vec2 ap;
    vec2 ab;

    vec2_sub(ap, p, a);
    vec2_sub(ab, b, a);
    vec2_norm(ab, ab);
    vec2_scale(ab, ab, vec2_dot(a, b));
    vec2_add(n, a, ab);
}

float vec2_angle(const vec2 a, const vec2 b)
{
    float r;
    r = U_sqrtf(a[0] * a[0] + a[1] * a[1]) * U_sqrtf(b[0] * b[0] + b[1] * b[1]);
    U_ASSERT(r != 0.0f);
    r = (a[0] * b[0] + a[1] * b[1]) / r; /* cos(phi) */
    r = U_acosf(r);
    r = r * (180.0f / U_PI); /* radians to degree */
    return r;
}

void vec2_proj(vec2 r, const vec2 a, const vec2 b)
{
    float d1;
    float d2;

    d1 = vec2_dot(a, b);
    d2 = vec2_dot(b, b); /* should be always 1.0 ??? */

    vec2_scale(r, b, d1 / d2);
}

/* normal of 'v', 90 degree anti-clockwise */
void vec2_perpendicular(vec2 r, const vec2 v)
{
    float tmp;

    tmp = v[0];
    r[0] = -v[1];
    r[1] = tmp;
}

/*
    https://cp-algorithms.com/geometry/oriented-triangle-area.html

    "Given three points p1, p2 and p3, calculate an oriented (signed) area of a triangle formed by them.
     The sign of the area is determined in the following way: imagine you are standing in the plane
     at point p1 and are facing p2. You go to p2 and if p3 is to your right (then we say the three vectors turn
     'clockwise'), the sign of the area is negative, otherwise it is positive. If the three points
     are collinear, the area is zero."

     (x2 - x1)(y3 - y2) - (x3 - x2)(y2 - y1)

     < 0 clockwise
     > 0 anti-clockwise
       0 collinear
*/
float vec2_signed_area(const vec2 p1, const vec2 p2, const vec2 p3)
{
    return (p2[0] - p1[0]) * (p3[1] - p2[1]) - (p3[0] - p2[0]) * (p2[1] - p1[1]);
}

void CartesianToPolar(vec2 pol, const vec2 cartesian)
{
    float r;
    float phi;
    float xzero;

    r = vec2_len(cartesian);
    xzero = cartesian[1] > 0.0 ? U_PI * 0.5 : U_PI * -0.5;
    phi = U_atan2f(cartesian[1], cartesian[0]);

    phi = cartesian[0] == 0.0 ? xzero : phi;
    phi = (phi < 0.0 ? phi + 2.0 * U_PI : phi);
    pol[0] = r;
    pol[1] = phi;
}

void PolarToCartesian(vec2 cartesian, const vec2 pol)
{
    cartesian[0] = pol[0] * U_cosf(pol[1]);
    cartesian[1] = pol[0] * U_sinf(pol[1]);
}

#endif /* LINMATH_H */

void U_WriteImagePPM(const char *path, const u8 *data, unsigned channels, unsigned width, unsigned height)
{
    FILE *f;
    unsigned x;
    unsigned y;

    unsigned r;
    unsigned g;
    unsigned b;

    if (!(channels == 1 || channels == 3 || channels == 4))
        return;


    U_Printf("write: %s\n", path);


    f = fopen(path, "w");

    if (!f)
        return;

    fprintf(f, "P3\n%u %u\n255\n", width, height);

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            r = data[y * height + x * channels];

            if (channels == 3 || channels == 4)
            {
                g = data[y * height + x * channels + 1];
                b = data[y * height + x * channels + 2];
            }
            else
            {
                g = r;
                b = r;
            }

            fprintf(f, "%u %u %u\n", r, g, b);
        }
    }

    fprintf(f, "\n");

    fclose(f);
}
