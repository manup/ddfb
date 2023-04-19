
#include <io.h>
#include <direct.h>
#include <windows.h>
#include <bcrypt.h>
#include <sys/stat.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif

static char _pl_config_dir_path[256];

#ifndef _PL_FILL_RANDOM
#define _PL_FILL_RANDOM
int PL_FillRandom(unsigned char *data, unsigned int size)
{
    NTSTATUS res;
    res = BCryptGenRandom(NULL, data, size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!NT_SUCCESS(res) || size > ULONG_MAX) {
        return 0;
    } else {
        return 1;
    }
}
#endif

#ifndef _PL_LOAD_FILE_
#define _PL_LOAD_FILE_
int PL_LoadFile(const char *path, void *buf, unsigned bufsize)
{
    U_ASSERT(path);
    U_ASSERT(buf);
    U_ASSERT(bufsize > 0);

    errno_t err;

    FILE *f;

    err = fopen_s(&f, path, "rb, ccs=UTF-8");

    if (!f)
        return 0;

    U_ASSERT(err == 0);

    fseek(f, 0L, SEEK_END);

    long size = ftell(f);

    fseek(f, 0L, SEEK_SET);

    if (size < (long)bufsize)
    {
        ((char*)buf)[size] = '\0';
        size_t n = fread(buf, 1, size, f);
        if ((long)n != size)
        {
            size = 0; // err
        }
    }
    else
    {
        size = 0; // too large
    }

    fclose(f);

    return size;
}
#endif

#ifndef _PL_GET_TIME_
#define _PL_GET_TIME_
double PL_GetTime()
{
    LARGE_INTEGER now;
    LARGE_INTEGER s_frequency;
    BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
    if (s_use_qpc)
    {
        QueryPerformanceCounter(&now);
        return (double)((1000LL * now.QuadPart) / s_frequency.QuadPart);
    }
    else
    {
        return GetTickCount();
    }
}
#endif

#if !defined _PL_REALPATH
#define _PL_REALPATH
int PL_RealPath(const char *path, char *resolved, unsigned bufsize)
{
    /* https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfullpathnamea */
    if (GetFullPathNameA(path, (DWORD)bufsize, resolved, NULL) != 0)
        return 1;
    return 0;
}
#endif

#ifndef _PL_WRITE_FILE_
#define _PL_WRITE_FILE_
int PL_WriteFile(const char *path, const void *buf, unsigned bufsize)
{
    U_ASSERT(path);
    U_ASSERT(buf);
    U_ASSERT(bufsize > 8);

    errno_t err;

    FILE *f;
    size_t n;

    if (bufsize == 0)
        return -1;

    err = fopen_s(&f, path, "wb");

    if (!f || err)
        return -1;

    n = fwrite(buf, 1, (size_t)bufsize, f);
    fclose(f);

    return (size_t)bufsize == n ? 1 : -1;
}
#endif

#ifndef _PL_FILE_EXISTS
#define _PL_FILE_EXISTS
int PL_FileExists(const char *path)
{
    U_ASSERT(path);
    if (_access(path, 0) != 0)
        return 0;

    return 1;
}
#endif

#ifndef _PL_MOVE_FILE
#define _PL_MOVE_FILE
int PL_MoveFile(const char *src, const char *dst)
{
    U_ASSERT(src);
    U_ASSERT(dst);

    if (!PL_FileExists(src))
        return 0;

    if (PL_FileExists(dst))
        return 0;

    if (rename(src, dst) == 0)
        return 1;

    U_Printf("PL_MoveFile: %s -> %s, failed: %s\n", src, dst, strerror(errno));

    return -1;
}
#endif

#ifndef _PL_DELETE_FILE
#define _PL_DELETE_FILE
int PL_DeleteFile(const char *path)
{
    U_ASSERT(path);

    if (!PL_FileExists(path))
        return 0;

    if (_unlink(path) == 0)
        return 1;

    U_Printf("PL_DeleteFile: %s, failed: %s\n", path, strerror(errno));
    return 0;
}
#endif

#ifndef _PL_MAKE_DIRECTORY
#define _PL_MAKE_DIRECTORY
int PL_MakeDirectory(const char *path)
{
    if (PL_FileExists(path))
        return 1;

    if (_mkdir(path) == 0)
        return 1;

    U_Printf("PL_MakeDirectory: %s, failed: %s\n", path, strerror(errno));

    return 0;
}
#endif

int PL_StatFile(const char *path, PL_Stat *st)
{
    int ret;
    struct stat sb;

    ret = stat(path, &sb);


    if (ret == 0)
    {
        st->size = sb.st_size;
        st->mtime = sb.st_mtime * 1000;
        return 1;
    }

    st->size = 0;
    st->mtime = 0;

    /*U_Printf("PL_Stat: %s, failed: %s\n", path, strerror(errno));*/

    return 0;
}

#ifndef _PL_CONFIG_DIR
#define _PL_CONFIG_DIR
const char *PL_ConfigDir(void)
{
    U_SStream ss;
    const char *p;

    if (_pl_config_dir_path[0])
        return &_pl_config_dir_path[0];

    U_sstream_init(&ss, &_pl_config_dir_path[0], sizeof(_pl_config_dir_path));

#if 0
    p = getenv("XDG_DATA_HOME");

    if (p)
    {
        U_sstream_put_str(&ss, p);
    }
    else
    {
        p = getenv("HOME");
        if (p)
        {
            U_sstream_put_str(&ss, p);
            U_sstream_put_str(&ss, "/.local/share");
        }
    }

    if (ss.pos == 0)
        goto err;

    if (!PL_FileExists(ss.str))
    {
        U_Printf("PL_ConfigDir: %s doesn't exists\n", ss.str);
        goto err;
    }

    U_sstream_put_str(&ss, "/dresden-elektronik");
    if (!PL_FileExists(ss.str) && !PL_MakeDirectory(ss.str))
        goto err;

    U_sstream_put_str(&ss, "/thg");
    if (!PL_FileExists(ss.str) && !PL_MakeDirectory(ss.str))
        goto err;

    return &_pl_config_dir_path[0];
err:
#endif
    U_UNUSED(p);

    U_Printf("PL_ConfigDir: failed to determine config location\n");
    _pl_config_dir_path[0] = '\0';
    return &_pl_config_dir_path[0];
}
#endif

#ifndef _PL_GET_CONFIG_FILE_PATH
#define _PL_GET_CONFIG_FILE_PATH
int PL_GetConfigFilePath(U_SStream *ss, const char *fname)
{
    U_ASSERT(fname);
    U_ASSERT(fname[0]);

    const char *path;

    path = PL_ConfigDir();
    if (path[0] == 0)
        return 0;

    U_sstream_put_str(ss, path);
    U_sstream_put_str(ss, "/");
    U_sstream_put_str(ss, fname);
    return 1;
}
#endif
