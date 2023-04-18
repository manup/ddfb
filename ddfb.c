#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>

#ifdef PL_POSIX
  #include <unistd.h>
#endif

#include <time.h>

#include "utils/u_types.h"
#include "utils/u_assert.h"
#include "utils/u_sstream.h"
#include "utils/u_bstream.h"
#include "utils/u_math.h"
#include "utils/u_memory.h"
#include "utils/u_arena.h"
#include "utils/u_scratch.h"
#include "utils/utils.h"
#include "utils/cj.h"

#include "uECC.h"

#define LONESHA256_STATIC
#include "vendor/lonesha256.h"

#include "utils/utils.c"
#include "utils/u_arena.c"
#include "utils/u_math.c"
#include "utils/u_memory.c"
#include "utils/u_scratch.c"
#include "utils/u_sstream.c"
#include "utils/u_bstream.c"
#include "utils/utils_time.c"
#include "utils/cj.c"


/*

   Cross compilation

   MinGW-x64:    ./dockcross-windows-static-x64 bash -c 'make CC=x86_64-w64-mingw32.static-gcc clean all'

*/

/* https://github.com/deconz-community/ddf-tools/blob/main/packages/bundler/README.md */

#define U_PATH_MAX PATH_MAX
#define CJ_MAX_TOKENS 1048576
#define VAL_BUF_SIZE 4096

typedef struct
{
  int size;
  char mtime[32]; /* 2023-01-08T17:24:24Z */
  u8 *mem;
} extfile;

typedef struct
{
    u32 tag;
    u32 size;
    u32 offset; /* where chunk data begins */
} ChunkRef;

typedef struct DDF_Signature
{
    u8 compressed_pubkey[33];
    u8 serialized_signature[64];
} DDF_Signature;

/* list of generic items (without duplicates) in mem_arena */
static u32 generic_item_cache_count;
static unsigned long generic_item_cache[1024];

static U_Arena mem_arena; /* for non scratch memory */

static void print_hex(unsigned char *data, unsigned size)
{
    while(size)
    {
        U_Printf("%02X", *data);
        data++;
        size--;
    }
}

static void DDF_PutFourCC(U_BStream *bs, const char *tag)
{
    int i;
    unsigned len;

    len = U_strlen(tag);
    U_ASSERT(len == 4);

    if (len != 4)
        tag = "ERR0";

    for (i = 0; i < 4; i++)
    {
        U_bstream_put_u8(bs, (u8)*tag);
        tag++;
    }
}

static extfile DDF_ResolveExtFile(const char *abs_path, const char *ext_path)
{
    extfile f;
    int size;
    unsigned i;
    unsigned j;
    char *ext_abs_path;
    PL_Stat statbuf;

    ext_abs_path = U_ScratchAlloc(U_PATH_MAX);
    U_ASSERT(ext_abs_path);

    i = U_strlen(abs_path);
    U_ASSERT(i > 0);
    for (;i && abs_path[i] != '/'; i--)
        ;

    if (i)
    {
        U_memcpy(ext_abs_path, abs_path, i);
        ext_abs_path[i++] = '/';
        ext_abs_path[i] = '\0';
    }

    for (j = 0; ext_path[j]; j++)
        ext_abs_path[i + j] = ext_path[j];

    ext_abs_path[i + j] = '\0';

    U_bzero(&f, sizeof(f));

    if (ext_abs_path[0])
    {
        size = U_MEGA_BYTES(1);
        f.mem = U_ScratchAlloc(size);
        f.size = PL_LoadFile(&ext_abs_path[0], f.mem, size);

        PL_StatFile(&ext_abs_path[0], &statbuf);

        if (U_TimeToISO8601_UTC(statbuf.mtime, &f.mtime[0], sizeof(f.mtime)))
        {
            /* cut off milliseconds .000Z */
            f.mtime[19] = 'Z';
            f.mtime[20] = '\0';
        }
    }

    return f;
}

static int DDF_StoreBundle(const char *path, U_BStream *bs)
{
    char *bundle_path;
    unsigned i;
    unsigned j;
    unsigned sz;

    if (bs->status != U_BSTREAM_OK)
        return 0;

    sz = U_strlen(path);
    for (i = U_strlen(path); i && path[i - 1] != '/'; --i)
        ;

    sz = U_strlen(&path[i]);
    bundle_path = U_ScratchAlloc(sz + 16);

    for (j = 0; i && path[i]; i++, j++)
        bundle_path[j] = path[i];

    bundle_path[j] = '\0';

    if (bundle_path[j - 5] != '.')
    {
        U_Printf("path file extension '.' not found in %s\n", path);
        return 0;
    }

    j -= 4;
    bundle_path[j++] = 'd';
    bundle_path[j++] = 'd';
    bundle_path[j++] = 'f';
    bundle_path[j++] = '\0';

    /* write file size in RIFF header */
    sz = bs->pos;
    bs->pos = 4;
    U_bstream_put_u32_le(bs, sz - 8);
    bs->pos = sz;

    if (PL_WriteFile(bundle_path, bs->data, sz) == 1)
    {
        U_Printf("bundle written to: %s (%u bytes)\n", bundle_path, bs->pos);
        return 1;
    }

    U_Printf("failed to write bundle to: %s (%u bytes)\n", bundle_path, bs->pos);
    return 0;
}

static int cj_is_valid_ref(cj_ctx *cj, cj_token_ref ref)
{
    if (ref >= 0 && ref < (cj_token_ref)cj->tokens_pos)
        return 1;
    return 0;
}

static int cj_is_array(cj_ctx *cj, cj_token_ref ref)
{
    if (cj_is_valid_ref(cj, ref))
    {
        if (cj->tokens[ref].type == CJ_TOKEN_ARRAY_BEG)
            return 1;
    }

    return 0;
}

static void U_sstream_put_js_str(U_SStream *ss, const char *str)
{
    U_sstream_put_str(ss, "\"");
    U_sstream_put_str(ss, str);
    U_sstream_put_str(ss, "\"");
}

static int DDF_MakeDescriptor(const char *path, u8 *ddf, int ddf_size, U_SStream *ss)
{
    cj_ctx cj;
    cj_token_ref ref_modelid0;
    cj_token_ref ref_modelid1;
    cj_token_ref ref_mfname0;
    cj_token_ref ref_mfname1;
    char *valbuf;
    int devid_count;
    PL_Stat statbuf;
    unsigned scratch_pos;

    scratch_pos = U_ScratchPos();

    U_ASSERT(ss->pos == 0);

    valbuf = U_ScratchAlloc(VAL_BUF_SIZE);

    /* start descriptor object */
    U_sstream_put_str(ss, "{");

    /* parse JSON */
    cj.tokens = U_ScratchAlloc(CJ_MAX_TOKENS * sizeof(cj_token));
    cj_parse_init(&cj, (char*)ddf, (unsigned)ddf_size, cj.tokens, CJ_MAX_TOKENS);

    cj_parse(&cj);
    if (cj.status != CJ_OK)
    {
        U_Printf("failed to parse JSON, status: %d\n", (int)cj.status);
        goto err;
    }

    /* version (required) */
    if (cj_copy_value(&cj, valbuf, VAL_BUF_SIZE, 0, "version"))
    {
        U_sstream_put_js_str(ss, "version");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, valbuf);
        U_sstream_put_str(ss, ",");
    }
    else
    {
        /* temporary use version "0.0.0" until we have actual versions */
        U_sstream_put_js_str(ss, "version");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, "0.0.0");
        U_sstream_put_str(ss, ",");
    }
    /*
    else
    {
        U_Printf("key 'version' not found\n");
        goto err;
    }
    */

    /*** version_deconz (required) ***********************************/
    if (cj_copy_value(&cj, valbuf, VAL_BUF_SIZE, 0, "version_deconz"))
    {
        U_sstream_put_js_str(ss, "version_deconz");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, valbuf);
        U_sstream_put_str(ss, ",");
    }
    else
    {
        /* temporary use version "2.19.3" until we have actual versions */
        U_sstream_put_js_str(ss, "version_deconz");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, ">2.19.3");
        U_sstream_put_str(ss, ",");
    }
    /*
    else
    {
        U_Printf("key 'version_deconz' not found\n");
        goto err;
    }
    */

    /*** last_modified (required) ************************************/
    PL_StatFile(path, &statbuf);

    if (U_TimeToISO8601_UTC(statbuf.mtime, valbuf, VAL_BUF_SIZE))
    {
        /* cut off milliseconds .000Z */
        valbuf[19] = 'Z';
        valbuf[20] = '\0';
        U_sstream_put_js_str(ss, "last_modified");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, valbuf);
        U_sstream_put_str(ss, ",");
    }


    /* product (required) */
    if (cj_copy_value(&cj, valbuf, VAL_BUF_SIZE, 0, "product"))
    {
        U_sstream_put_js_str(ss, "product");
        U_sstream_put_str(ss, ":");
        U_sstream_put_js_str(ss, valbuf);
        U_sstream_put_str(ss, ",");
    }
    else
    {
        U_Printf("key 'product' not found\n");
        goto err;
    }

    /*** device_identifiers (required) *********************************/
    /*
        Processes both, modelid and mfname, arrays in parallel.
    */
    ref_modelid0 = cj_value_ref(&cj, 0, "modelid");
    if (cj_is_valid_ref(&cj, ref_modelid0) == 0)
    {
        U_Printf("key 'modelid' not found\n");
        goto err;
    }

    ref_mfname0 = cj_value_ref(&cj, 0, "manufacturername");
    if (cj_is_valid_ref(&cj, ref_mfname0) == 0)
    {
        U_Printf("key 'manufacturername' not found\n");
        goto err;
    }

    U_sstream_put_js_str(ss, "device_identifiers");
    U_sstream_put_str(ss, ":[");

    devid_count = 0;
    if (cj_is_array(&cj, ref_modelid0) && cj_is_array(&cj, ref_mfname0))
    {
        ref_modelid1 = ref_modelid0 + 1;
        ref_mfname1 = ref_mfname0 + 1;

        for (; ;ref_modelid1++, ref_mfname1++)
        {
            if (cj_is_valid_ref(&cj, ref_modelid1) == 0)
                goto err_invalid_model_mfname;

            if (cj_is_valid_ref(&cj, ref_mfname1) == 0)
                goto err_invalid_model_mfname;

            /* arrays must have equal arity */
            if (cj.tokens[ref_modelid1].type != cj.tokens[ref_mfname1].type)
                goto err_invalid_model_mfname;

            if (cj.tokens[ref_modelid1].type == CJ_TOKEN_ARRAY_END)
                break;

            if (cj.tokens[ref_modelid1].parent != ref_modelid0)
                goto err_invalid_model_mfname;

            if (cj.tokens[ref_mfname1].parent != ref_mfname0)
                goto err_invalid_model_mfname;

            if (cj.tokens[ref_modelid1].type == CJ_TOKEN_ITEM_SEP)
                continue;

            if (cj.tokens[ref_modelid1].type != CJ_TOKEN_STRING)
                goto err_invalid_model_mfname;


            if (devid_count > 0)
                U_sstream_put_str(ss, ",");

            /* [ mfname, modelid ] */
            U_sstream_put_str(ss, "[");

            if (cj_copy_ref(&cj, valbuf, VAL_BUF_SIZE, ref_mfname1) == 0)
                goto err_invalid_model_mfname;
            U_sstream_put_js_str(ss, valbuf);

            U_sstream_put_str(ss, ",");

            if (cj_copy_ref(&cj, valbuf, VAL_BUF_SIZE, ref_modelid1) == 0)
                goto err_invalid_model_mfname;
            U_sstream_put_js_str(ss, valbuf);

            U_sstream_put_str(ss, "]");

            devid_count++;
        }
    }
    else if (cj.tokens[ref_modelid0].type == CJ_TOKEN_STRING &&
             cj.tokens[ref_mfname0].type == CJ_TOKEN_STRING)
    {
        /* [ mfname, modelid ] */
        U_sstream_put_str(ss, "[");

        if (cj_copy_ref(&cj, valbuf, VAL_BUF_SIZE, ref_mfname0) == 0)
            goto err_invalid_model_mfname;
        U_sstream_put_js_str(ss, valbuf);

        U_sstream_put_str(ss, ",");

        if (cj_copy_ref(&cj, valbuf, VAL_BUF_SIZE, ref_modelid0) == 0)
            goto err_invalid_model_mfname;
        U_sstream_put_js_str(ss, valbuf);

        U_sstream_put_str(ss, "]");

        devid_count++;
    }
    else
    {
        goto err_invalid_model_mfname;
    }

    if (devid_count == 0)
        goto err_invalid_model_mfname;

    /*** end device_identifiers ********************************************************/
    U_sstream_put_str(ss, "]");

    /* end descriptor object */
    U_sstream_put_str(ss, "}");

    U_Printf("\n%s\n", ss->str);

    U_ScratchRestore(scratch_pos);
    return 1;

err:
    return 0;

err_invalid_model_mfname:
    U_Printf("key 'manufacturername' or 'modelid' invalid\n");
    return 0;
}

static int DDF_ResolveGenericItem(const char *generic_items_path, const char *item_name, U_BStream *bs)
{
    u8 *data;
    unsigned i;
    char *item_path;
    const char *rel_path;
    unsigned rel_path_start;
    unsigned scratch_pos;
    unsigned long hash;
    unsigned extf_size_pos;
    char mtime[32];
    PL_Stat statbuf;
    U_SStream ss;

    scratch_pos = U_ScratchPos();

    item_path = U_ScratchAlloc(U_PATH_MAX);

    U_sstream_init(&ss, item_path, U_PATH_MAX);
    U_sstream_put_str(&ss, generic_items_path);
    U_sstream_put_str(&ss, "/");

    i = ss.pos;
    rel_path_start = i - U_strlen("generic/items") - 1;
    rel_path = &item_path[rel_path_start];
    U_sstream_put_str(&ss, item_name);

    for (; i < ss.pos; i++)
    {
        if (ss.str[i] == '/')
            ss.str[i] = '_';
    }

    U_sstream_put_str(&ss, "_item.json");

    /* mark loaded files by remember the hash of relative path */
    hash = U_hash_djb2(rel_path, U_strlen(rel_path));

    for (i = 0; i < generic_item_cache_count; i++)
    {
        if (generic_item_cache[i] == hash)
        {
            /*U_Printf("%s already known\n", rel_path);*/
            U_ScratchRestore(scratch_pos);
            return 1;
        }
    }

    generic_item_cache[generic_item_cache_count] = hash;
    generic_item_cache_count++;

    if (PL_StatFile(item_path, &statbuf) == 0)
    {
        U_Printf("failed to resolve '%s'\n", item_path);
        return 0;
    }

    data = U_ScratchAlloc(statbuf.size + 16);

    if (PL_LoadFile(item_path, data, statbuf.size + 16) != (int)statbuf.size)
    {
        U_Printf("failed to load: %s\n", item_path);
        return 0;
    }

    /*U_Printf("add '%s' %u bytes\n", rel_path, statbuf.size);*/

    /*****************************************************************/

    DDF_PutFourCC(bs, "EXTF");
    extf_size_pos = bs->pos;
    U_bstream_put_u32_le(bs, 0); /* chunk size dummy */

    DDF_PutFourCC(bs, "JSON"); /* file type */

    /* put path + '\0' */
    U_bstream_put_u16_le(bs, U_strlen(rel_path) + 1);
    for (i = 0; i < U_strlen(rel_path) + 1; i++)
        U_bstream_put_u8(bs, rel_path[i]);


    if (U_TimeToISO8601_UTC(statbuf.mtime, &mtime[0], sizeof(mtime)))
    {
        /* cut off milliseconds .000Z */
        mtime[19] = 'Z';
        mtime[20] = '\0';
    }

    /* modification time length incl. '\0' */
    U_bstream_put_u16_le(bs, U_strlen(&mtime[0]) + 1);
    /* modification time in ISO 8601 format */
    for (i = 0; mtime[i]; i++)
        U_bstream_put_u8(bs, (u8)mtime[i]);
    U_bstream_put_u8(bs, 0); /* '\0' */

    U_bstream_put_u32_le(bs, statbuf.size);
    for (i = 0; i < statbuf.size; i++)
        U_bstream_put_u8(bs, data[i]);

    /* EXTF chunk size */
    i = (int)bs->pos;
    bs->pos = extf_size_pos;
    U_bstream_put_u32_le(bs, i - ((int)extf_size_pos + 4));
    bs->pos = (unsigned)i;

    U_ScratchRestore(scratch_pos);

    return 1;
}

static int DDF_AddGenericItems(const char *abs_path, u8 *ddf, int ddf_size, U_BStream *bs)
{
    unsigned i;
    cj_ctx cj;
    cj_token *tok;
    cj_token_ref ref_subdevices;
    cj_token_ref ref_subdev;
    cj_token_ref ref_items;
    cj_token_ref ref_item_name;
    char *valbuf;
    PL_Stat statbuf;
    unsigned scratch_pos;
    unsigned tok_pos;
    char *generic_items_path;
    const char *test_path;

    scratch_pos = U_ScratchPos();

    valbuf = U_ScratchAlloc(VAL_BUF_SIZE);


    /*** search generic/items directory ******************************/
    /* walk dir tree from DDF up and look for generic/items/attr_id_item.json file.
     */
    generic_items_path = U_ScratchAlloc(U_PATH_MAX);
    generic_items_path[0] = '\0';

    i = U_strlen(abs_path);
    U_memcpy(generic_items_path, abs_path, U_strlen(abs_path) + 1);

    test_path = "generic/items/attr_id_item.json";
    for (;i; i--)
    {
        if (generic_items_path[i - 1] == '/')
        {
            U_memcpy(&generic_items_path[i], test_path, U_strlen(test_path) + 1);

            if (PL_StatFile(generic_items_path, &statbuf) != 0)
                break;
        }
    }

    if (i == 0)
    {
        U_Printf("failed to find generic items directory\n");
        goto err;
    }

    U_memcpy(&generic_items_path[i], "generic/items", U_strlen("generic/items") + 1);

    /*** parse JSON **************************************************/
    cj.tokens = U_ScratchAlloc(CJ_MAX_TOKENS * sizeof(cj_token));
    cj_parse_init(&cj, (char*)ddf, (unsigned)ddf_size, cj.tokens, CJ_MAX_TOKENS);

    cj_parse(&cj);
    if (cj.status != CJ_OK)
    {
        U_Printf("failed to parse JSON, status: %d\n", (int)cj.status);
        goto err;
    }

    ref_subdevices = cj_value_ref(&cj, 0, "subdevices");
    if (cj_is_valid_ref(&cj, ref_subdevices) == 0)
    {
        U_Printf("key 'subdevices' not found\n");
        goto err;
    }

    for (tok_pos = ref_subdevices; tok_pos < cj.tokens_pos; tok_pos++)
    {
        tok = &cj.tokens[tok_pos];
        if (tok->parent == ref_subdevices && tok->type == CJ_TOKEN_OBJECT_BEG)
        {
            ref_subdev = tok_pos;

            ref_items = cj_value_ref(&cj, ref_subdev, "items");
            if (cj_is_valid_ref(&cj, ref_items) == 0)
            {
                U_Printf("key 'items' not found\n");
                goto err;
            }

            for (tok_pos = ref_items; tok_pos < cj.tokens_pos; tok_pos++)
            {
                tok = &cj.tokens[tok_pos];

                if (tok->parent == ref_subdev && tok->type == CJ_TOKEN_OBJECT_END)
                    break; /* end of items array */

                if (tok->parent == ref_items && tok->type == CJ_TOKEN_OBJECT_BEG)
                {
                    ref_item_name = cj_value_ref(&cj, tok_pos, "name");
                    if (cj_is_valid_ref(&cj, ref_item_name) == 0)
                    {
                        U_Printf("key item.'name' not found\n");
                        goto err;
                    }

                    if (cj_copy_ref(&cj, valbuf, VAL_BUF_SIZE, ref_item_name) == 0)
                        goto err;

                    if (DDF_ResolveGenericItem(generic_items_path, valbuf, bs) == 0)
                    {
                        U_Printf("failed to resolve file for generic item: %s\n", valbuf);
                        goto err;
                    }
                }
            }
        }
    }

    U_ScratchRestore(scratch_pos);
    return 1;

err:
    U_ScratchRestore(scratch_pos);
    return 0;
}

static int DDF_CreateBundle(const char *path)
{
    u8 *ddf;
    U_SStream ss;
    U_BStream bs;
    extfile extf;

    u32 ddfb_size_pos;
    int beg;
    int end;
    int ddf_size;
    int tsize = U_MEGA_BYTES(1);
    int slen;
    int i;
    char *str;
    char *abs_path;
    unsigned extf_size_pos;

    generic_item_cache_count = 0;

    abs_path = U_ScratchAlloc(U_PATH_MAX);
    U_ASSERT(abs_path);

    if (!PL_RealPath(path, abs_path, U_PATH_MAX))
    {
        U_Printf("failed to resolve: %s\n", path);
        return 0;
    }

    ddf = U_ScratchAlloc(tsize);
    U_ASSERT(ddf);

    ddf_size = PL_LoadFile(abs_path, ddf, tsize);
    if (ddf_size <= 0)
    {
        U_Printf("failed to read: %s\n", path);
        return 0;
    }

    /* RIFF encoded output*/
    bs.size = U_MEGA_BYTES(1);
    bs.data = U_ScratchAlloc(bs.size);
    U_ASSERT(bs.data);
    U_bstream_init(&bs, bs.data, bs.size);

    /*** RIFF header *************************************************/
    DDF_PutFourCC(&bs, "RIFF");
    U_ASSERT(bs.pos == 4);
    U_bstream_put_u32_le(&bs, 0); /* dummy filled later */

    /*** DDFB header *************************************************/
    DDF_PutFourCC(&bs, "DDFB"); /* DDF_BUNDLE_MAGIC */
    U_ASSERT(bs.pos == 12);
    ddfb_size_pos = bs.pos;
    U_bstream_put_u32_le(&bs, 0); /* dummy filled later */

    /*** DESC chunk **************************************************/
    DDF_PutFourCC(&bs, "DESC");

    ss.len = U_KILO_BYTES(8192);
    ss.str = U_ScratchAlloc(ss.len);
    ss.pos = 0;

    if (DDF_MakeDescriptor(abs_path, ddf, ddf_size, &ss) == 0)
    {
        U_Printf("failed to make DESC chunk\n");
        return 0;
    }

    U_bstream_put_u32_le(&bs, ss.pos);
    U_ASSERT(bs.pos == 24);
    for (i = 0; i < (int)ss.pos; i++)
        U_bstream_put_u8(&bs, ss.str[i]);

    /*** DDFC chunk **************************************************/
    /* Aka the base DDF JSON file. */
    /* TODO compress */
    DDF_PutFourCC(&bs, "DDFC");
    U_bstream_put_u32_le(&bs, ddf_size);
    for (i = 0; i < ddf_size; i++)
        U_bstream_put_u8(&bs, ddf[i]);


    /*** EXTF chunk(s) ***********************************************/
    U_sstream_init(&ss, ddf, ddf_size);
    while (U_sstream_at_end(&ss) == 0)
    {
       if (U_sstream_starts_with(&ss, "\"script\""))
       {
          ss.pos += 8; /* move behind "script" */
          while (U_sstream_at_end(&ss) == 0 && U_sstream_peek_char(&ss) != '\"')
          {
            ss.pos++;
          }

          beg = 0;
          end = 0;
          if (U_sstream_peek_char(&ss) == '\"')
          {
            ss.pos++;
            beg = ss.pos;
          }

          while (U_sstream_at_end(&ss) == 0 && U_sstream_peek_char(&ss) != '\"')
          {
            ss.pos++;
          }

          if (U_sstream_peek_char(&ss) == '\"')
          {
            end = ss.pos;
            ss.pos++;
          }

          slen = end - beg;
          if (beg && end && slen > 0 && slen < 1024)
          {
            str = U_ScratchAlloc(slen + 1);
            U_ASSERT(str);
            U_memcpy(str, &ddf[beg], slen);
            str[slen] = '\0';

            extf = DDF_ResolveExtFile(abs_path, str);
            if (extf.size == 0)
            {
              U_Printf("failed to resolve %s\n", str);
              return 0;
            }
            else
            {
                U_Printf("resolved %s (%d bytes)\n", str, extf.size);
                DDF_PutFourCC(&bs, "EXTF");
                extf_size_pos = bs.pos;
                U_bstream_put_u32_le(&bs, 0); /* chunk size dummy */

                DDF_PutFourCC(&bs, "SCJS"); /* file type */

                /* put path + '\0' */
                U_bstream_put_u16_le(&bs, slen + 1);
                for (i = 0; i < slen + 1; i++)
                    U_bstream_put_u8(&bs, str[i]);

                /* modification time length incl. '\0' */
                U_bstream_put_u16_le(&bs, U_strlen(&extf.mtime[0]) + 1);
                /* modification time in ISO 8601 format */
                for (i = 0; extf.mtime[i]; i++)
                    U_bstream_put_u8(&bs, (u8)extf.mtime[i]);
                U_bstream_put_u8(&bs, 0); /* '\0' */

                U_bstream_put_u32_le(&bs, extf.size);
                for (i = 0; i < extf.size; i++)
                    U_bstream_put_u8(&bs, extf.mem[i]);

                /* EXTF chunk size */
                i = (int)bs.pos;
                bs.pos = extf_size_pos;
                U_bstream_put_u32_le(&bs, i - ((int)extf_size_pos + 4));
                bs.pos = (unsigned)i;
            }
          }
       }

       ss.pos++;
    }

    /*** EXTF chunk(s) generic items *********************************/
    if (DDF_AddGenericItems(abs_path, ddf, ddf_size, &bs) == 0)
    {
        U_Printf("failed to add generic items\n");
        return 0;
    }

    /* DDFB chunk size */
    i = (int)bs.pos;
    bs.pos = ddfb_size_pos;
    U_bstream_put_u32_le(&bs, i - ((int)ddfb_size_pos + 4));
    bs.pos = (unsigned)i;

    return DDF_StoreBundle(path, &bs);
}

static int uECC_RNG_Callback(uint8_t *dest, unsigned size)
{
    if (PL_FillRandom(dest, size) == 0)
        return 0;
    return 1;
}

int ECC_CreateKeyPair(const char *path)
{
    int result;
    U_SStream ss;
    const struct uECC_Curve_t * curve;
    unsigned char private_key[32];
    unsigned char public_key[64];
    unsigned char compressed_pubkey[33];
    char outpath[U_PATH_MAX];

    uECC_set_rng(uECC_RNG_Callback);

    curve = uECC_secp256k1();

    if (uECC_make_key(public_key, private_key, curve) != 1)
    {
        printf("uECC_make_key() failed\n");
        return 0;
    }

    uECC_compress(public_key, compressed_pubkey, curve);

    U_sstream_init(&ss, &outpath[0], sizeof(outpath));
    U_sstream_put_str(&ss, path);

    result = 0;
    if (PL_WriteFile(ss.str, private_key, sizeof(private_key)) == 1)
        result = 1;

    U_sstream_put_str(&ss, ".pub");
    if (PL_WriteFile(ss.str, compressed_pubkey, sizeof(compressed_pubkey)) == 1)
        result += 1;

    U_Printf("private key: ");
    print_hex(private_key, sizeof(private_key));
    U_Printf("\n");

    U_Printf("public key:  ");
    print_hex(compressed_pubkey, sizeof(compressed_pubkey));
    U_Printf("\n");

    return result == 2 ? 1 : 0;
}

static ChunkRef GetChunkRef(u8 *data, u32 size, u32 offset, const char *fourcc)
{
    u32 tag;
    u32 chunk_tag;
    u32 chunk_size;
    U_BStream bs;
    unsigned len;
    ChunkRef result;

    U_bzero(&result, sizeof(result));

    U_ASSERT(offset < size);
    U_bstream_init(&bs, data, size);
    bs.pos = offset;

    len = U_strlen(fourcc);
    U_ASSERT(len == 4);
    U_memcpy(&tag, fourcc, 4);

    for (;bs.status == U_BSTREAM_OK;)
    {
        chunk_tag = U_bstream_get_u32_le(&bs);
        chunk_size = U_bstream_get_u32_le(&bs);

        if (chunk_tag == tag)
        {
            result.tag = chunk_tag;
            result.size = chunk_size;
            result.offset = bs.pos;
            U_Printf("TAG: %s, offset: %u, size: %u\n", fourcc, bs.pos, chunk_size);
            break;
        }

        if (chunk_tag == 0)
            break;

        bs.pos += chunk_size;
        if (bs.pos >= bs.size)
            break;
    }

    return result;
}

int ECC_FindSignature(const DDF_Signature *sig, u8 *ddf_data, u32 ddf_size, u8 *sha256, u8 *public_key)
{
    u32 i;
    u16 len;
    int offset;
    U_BStream bs;
    ChunkRef chunk;
    DDF_Signature sig1;

    U_bstream_init(&bs, ddf_data, ddf_size);

    offset = 8; /* inside RIFF container */
    chunk = GetChunkRef(ddf_data, ddf_size, offset, "SIGN");

    if (chunk.tag == 0) /* no SIGN chunk */
    {
        return 0;
    }

    bs.pos = chunk.offset;

    for (;;)
    {
        if (bs.pos >= chunk.offset + chunk.size)
            break;

        len = U_bstream_get_u16_le(&bs);
        if (len != sizeof(sig1.compressed_pubkey))
        {
            U_Printf("unsupported public key length: %u\n", len);
            return 0;
        }

        for (i = 0; i < sizeof(sig1.compressed_pubkey); i++)
            sig1.compressed_pubkey[i] = U_bstream_get_u8(&bs);

        len = U_bstream_get_u16_le(&bs);
        if (len != sizeof(sig1.serialized_signature))
        {
            U_Printf("unsupported signature length: %u\n", len);
            return 0;
        }

        for (i = 0; i < sizeof(sig1.serialized_signature); i++)
            sig1.serialized_signature[i] = U_bstream_get_u8(&bs);

        if (U_memcmp(sig1.compressed_pubkey, sig->compressed_pubkey, sizeof(sig->compressed_pubkey)) == 0)
        {
            if (uECC_verify(public_key, sha256, 32, sig1.serialized_signature, uECC_secp256k1()) == 1)
                return 1;
        }
    }

    return 0;
}

int ECC_AppendSignature(const DDF_Signature *sig, u8 *ddf_data, u32 ddf_size, u32 *out_ddf_size)
{
    u32 i;
    u32 pos;
    int offset;
    U_BStream bs;
    ChunkRef chunk;

    U_bstream_init(&bs, ddf_data, ddf_size);

    offset = 8; /* inside RIFF container */
    chunk = GetChunkRef(ddf_data, ddf_size, offset, "SIGN");

    if (chunk.tag == 0) /* no SIGN chunk, append one */
    {
        chunk = GetChunkRef(ddf_data, ddf_size, offset, "DDFB");
        U_ASSERT(chunk.tag != 0);

        bs.pos = chunk.offset + chunk.size;
        DDF_PutFourCC(&bs, "SIGN");
        U_bstream_put_u32_le(&bs, 0); /* initial size*/

        chunk = GetChunkRef(ddf_data, ddf_size, offset, "SIGN");
    }

    U_ASSERT(chunk.tag != 0);

    /* append signature */
    bs.pos = chunk.offset + chunk.size;

    U_bstream_put_u16_le(&bs, sizeof(sig->compressed_pubkey));
    for (i = 0; i < sizeof(sig->compressed_pubkey); i++)
        U_bstream_put_u8(&bs, sig->compressed_pubkey[i]);

    U_bstream_put_u16_le(&bs, sizeof(sig->serialized_signature));
    for (i = 0; i < sizeof(sig->serialized_signature); i++)
        U_bstream_put_u8(&bs, sig->serialized_signature[i]);

    /* write new SIGN chunk size */
    pos = bs.pos;
    bs.pos = chunk.offset - 4;
    U_bstream_put_u32_le(&bs, pos - chunk.offset);

    /* update RIFF chunk size*/
    bs.pos = 4;
    U_bstream_put_u32_le(&bs, pos - 8);
    *out_ddf_size = pos;

    return 1;
}

int ECC_Sign(const char *ddfpath, const char *keypath)
{
    int ret;
    int offset;
    u32 ddf_size;
    u32 out_ddf_size;
    u8 *ddf_data;
    ChunkRef chunk;
    U_BStream bs;
    const struct uECC_Curve_t * curve;
    /* buffers */
    u8 sha256[32];
    u8 private_key[32 + 1];
    u8 public_key[64];
    DDF_Signature ddf_sig;

    PL_Stat statbuf;

    /*** load private key file ***************************************/
    if (PL_StatFile(keypath, &statbuf) != 1)
    {
        U_Printf("failed to open %s\n", keypath);
        return 0;
    }

    if (statbuf.size != 32)
    {
        U_Printf("failed to load private key: %s, expected 32 bytes, got %u\n", keypath, (unsigned)statbuf.size);
        return 0;
    }

    ret = PL_LoadFile(keypath, &private_key[0], sizeof(private_key));
    if (ret != (int)statbuf.size)
    {
        U_Printf("failed to load private key: %s, ret: %d\n", keypath, ret);
        return 0;
    }

    /*** load DDF file ***********************************************/
    if (PL_StatFile(ddfpath, &statbuf) != 1)
    {
        U_Printf("failed to open %s\n", ddfpath);
        return 0;
    }

    ddf_size = statbuf.size;
    ddf_size += 1024; /* some extra space for new signatures*/
    ddf_data = U_ScratchAlloc(ddf_size);
    ret = PL_LoadFile(ddfpath, ddf_data, ddf_size);
    if (ret != (int)statbuf.size)
    {
        U_Printf("failed to read %s (ret: %d), bytes: %u\n", ddfpath, ret, (unsigned)statbuf.size);
        return 0;
    }

    U_bstream_init(&bs, ddf_data, ddf_size);

    /*** test for valid DDF file *************************************/
    offset = 0;
    chunk = GetChunkRef(bs.data, bs.size, offset, "RIFF");
    if (chunk.tag == 0)
    {
        U_Printf("no RIFF chunk found in: %s\n", ddfpath);
        return 0;
    }

    offset = 8; /* inside RIFF container */
    chunk = GetChunkRef(bs.data, bs.size, offset, "DDFB");

    if (chunk.tag == 0)
    {
        U_Printf("no DDFB chunk found in: %s\n", ddfpath);
        return 0;
    }

    if (chunk.offset + chunk.size > statbuf.size)
    {
        U_Printf("invalid DDFB chunk size in: %s\n", ddfpath);
        return 0;
    }

    /*** generate SHA256 over DDFB chunk (header + data) *************/
    lonesha256(&sha256[0], &bs.data[chunk.offset - 8], chunk.size + 8);

    U_Printf("SHA256: ");
    print_hex(&sha256[0], sizeof(sha256));
    U_Printf("\n");

    /*** create signature over DDFB data *****************************/

    U_bzero(&ddf_sig, sizeof(ddf_sig));

    curve = uECC_secp256k1();
    uECC_set_rng(uECC_RNG_Callback);

    if (uECC_compute_public_key(private_key, public_key, curve) != 1)
    {
        U_Printf("failed to compute public key from %s\n", keypath);
        return 0;
    }

    U_Printf("private key: ");
    print_hex(&private_key[0], sizeof(private_key) - 1);
    U_Printf("\n");

    /* serialize the pubkey in a compressed form(33 bytes) */
    uECC_compress(public_key, ddf_sig.compressed_pubkey, curve);

    U_Printf("public key: ");
    print_hex(&ddf_sig.compressed_pubkey[0], sizeof(ddf_sig.compressed_pubkey));
    U_Printf("\n");

    /* signing */

    if (uECC_sign(private_key, sha256, sizeof(sha256), ddf_sig.serialized_signature, curve) != 1)
    {
        return 0;
    }

    U_Printf("signature: ");
    print_hex(ddf_sig.serialized_signature, sizeof(ddf_sig.serialized_signature));
    U_Printf("\n");

    /* check if signature is already there */
    if (ECC_FindSignature(&ddf_sig, ddf_data, statbuf.size, &sha256[0], &public_key[0]))
    {
        U_Printf("signature already present\n");
        return 1;
    }

    if (ECC_AppendSignature(&ddf_sig, ddf_data, ddf_size, &out_ddf_size))
    {
        PL_WriteFile(ddfpath, ddf_data, out_ddf_size);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int result;
    int arg_len;
    U_SStream ss;

    result = 1;
    U_MemoryInit();
    U_ScratchInit(U_MEGA_BYTES(32));
    U_InitArena(&mem_arena, U_MEGA_BYTES(16));

    ss.len = 2048;
    U_sstream_init(&ss, U_ScratchAlloc(ss.len), ss.len);

    U_sstream_put_str(&ss, argv[1]);
    arg_len = ss.pos;
    ss.pos = 0;

    if (argc == 3 && U_sstream_starts_with(&ss, "create") && arg_len == 6)
    {
        if (DDF_CreateBundle(argv[2]) == 1)
            result = 0;
    }
    else if (argc == 3 && U_sstream_starts_with(&ss, "keygen") && arg_len == 6)
    {
        if (ECC_CreateKeyPair(argv[2]) == 1)
            result = 0;
    }
    else if (argc == 4 && U_sstream_starts_with(&ss, "sign") && arg_len == 4)
    {
        if (ECC_Sign(argv[2], argv[3]) == 1)
            result = 0;
    }
    else
    {
        U_Printf("Usage: %s <command> <arguments...>\n", argv[0]);
        U_Printf("commands:\n");
        U_Printf("    create   <ddf.json>\n");
        U_Printf("             Creates a .ddf bundle from a base JSON DDF file.\n");
        U_Printf("    keygen   <keyname>\n");
        U_Printf("             Creates new key pair to sign bundles.\n");
        U_Printf("    sign     <bundle.ddf> <keyfile>\n");
        U_Printf("             Signs a bundle with a private key.\n");
        U_Printf("             The signature is appended only if it doesn't exist yet.\n");
        if (argc == 1)
            result = 0;
    }

    U_FreeArena(&mem_arena);
    U_ScratchFree();
    U_MemoryFree();

    return result;
}
