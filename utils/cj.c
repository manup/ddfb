/*
 * Copyright (c) 2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifdef NDEBUG
  #define CJ_ASSERT(c) ((void)0)
#else
  #if _MSC_VER
    #define CJ_ASSERT(c) if (!(c)) __debugbreak()
  #elif __GNUC__
    #define CJ_ASSERT(c) if (!(c)) __builtin_trap()
  #else
    #define CJ_ASSERT assert
  #endif
#endif /* NDEBUG */

/* Convert utf-8 to unicode code point.

   Returns >0 as number of bytes in utf8 character, and 'codepoint' set
   or 0 for invalid utf8, codepoint set to 0.

 */
static int cj_utf8_to_codepoint(const unsigned char *str, unsigned long len, unsigned long *codepoint)
{
    int result;
    unsigned long cp;
    unsigned bytes;

    result = 0;
    if (str && len != 0)
        cp = (unsigned)*str & 0xFF;
    else
        goto invalid;

    for (bytes = 0; cp & 0x80; bytes++)
        cp = (cp & 0x7F) << 1;

    if (bytes == 0) /* ASCII */
    {
        *codepoint = cp;
        return 1;
    }

    if (bytes > 4 || bytes > len)
        goto invalid;

    result = (int)bytes;

    /* 110xxxxx 10xxxxxx */
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    cp >>= bytes;
    bytes--;
    str++;

    for (;bytes; bytes--, str++)
    {
        if (((unsigned)*str & 0xC0) != 0x80) /* must start with 10xxxxxx */
            goto invalid;

        cp <<= 6;
        cp |= (unsigned)*str & 0x3F;
    }

    *codepoint = cp;
    return result;

invalid:
    *codepoint = 0;
    return 0;
}

static cj_status cj_is_valid_utf8(const unsigned char *str, unsigned long len)
{
    int ch_count;
    unsigned long codepoint;

    do /* test valid utf8 codepoints */
    {
        ch_count = cj_utf8_to_codepoint(str, len, &codepoint);
        if (ch_count > 0)
        {
            str += ch_count;
            len -= ch_count;
        }
        else
        {
            return CJ_INVALID_UTF8;
        }
    } while (ch_count > 0 && len > 0);

    return CJ_OK;
}

static unsigned cj_eat_white_space(const unsigned char *str, unsigned long len)
{
    unsigned long result;

    result = 0;

    for (; str[result] && result < len; )
    {
        switch(str[result])
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            result++;
            break;
        case '\0':
        default:
            goto out;
        }
    }

out:
    return result;
}

static int cj_is_primitive_char(unsigned char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '.' || c == '-') ? 1 : 0;
}

static int cj_alloc_token(cj_ctx *ctx, cj_token **tok)
{
    int result;

    result = CJ_INVALID_TOKEN_INDEX;

    if (ctx->tokens_pos < ctx->tokens_size)
    {
        result = ctx->tokens_pos;
        ctx->tokens_pos++;

        *tok = &ctx->tokens[result];
        (*tok)->type = CJ_TOKEN_INVALID;
        (*tok)->parent = CJ_INVALID_TOKEN_INDEX;
    }
    else
    {
        *tok = 0;
    }

    return result;
}

static unsigned cj_next_token(const unsigned char *str, unsigned len, unsigned pos, cj_token *tok)
{
    CJ_ASSERT(pos <= len);
    pos += cj_eat_white_space(&str[pos], len - pos);

    tok->type = CJ_TOKEN_INVALID;
    tok->pos = pos;
    tok->len = 0;

    for (;pos < len; pos++)
    {
        if (tok->type == CJ_TOKEN_INVALID)
        {
            switch (str[pos])
            {
                case '{': tok->type = CJ_TOKEN_OBJECT_BEG; tok->len = 1; return ++pos;
                case '}': tok->type = CJ_TOKEN_OBJECT_END; tok->len = 1; return ++pos;
                case '[': tok->type = CJ_TOKEN_ARRAY_BEG;  tok->len = 1; return ++pos;
                case ']': tok->type = CJ_TOKEN_ARRAY_END;  tok->len = 1; return ++pos;
                case ',': tok->type = CJ_TOKEN_ITEM_SEP;   tok->len = 1; return ++pos;
                case ':': tok->type = CJ_TOKEN_NAME_SEP;   tok->len = 1; return ++pos;
                case '\"': tok->type = CJ_TOKEN_STRING; tok->pos++; break;
                default:
                    break;
            }

            if (tok->type != CJ_TOKEN_INVALID)
                continue;

            if (cj_is_primitive_char(str[pos]))
            {
                tok->type = CJ_TOKEN_PRIMITIVE;
                tok->len = 1;
            }
            else
            {
                break;
            }
        }
        else if (tok->type == CJ_TOKEN_STRING)
        {
            if (str[pos] == '\"' && str[pos - 1] != '\\')
                return ++pos;
            tok->len++;
        }
        else if (tok->type == CJ_TOKEN_PRIMITIVE)
        {
            if (!cj_is_primitive_char(str[pos]))
            {
                break;
            }
            tok->len++;
        }
        else
        {
            break;
        }
    }

    return pos;
}

static void cj_trim_trailing_whitespace(cj_ctx *ctx)
{
    for (;ctx->size > 0;)
    {
        switch(ctx->buf[ctx->size - 1])
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\0':
            ctx->size--;
            break;
        default:
            return;
        }
    }
}

void cj_parse_init(cj_ctx *ctx, const char *json, unsigned len,
                   cj_token *tokens, unsigned tokens_size)
{
    if (!ctx)
        return;

    ctx->buf = (const unsigned char*)json;
    ctx->pos = 0;
    ctx->size = len;

    ctx->tokens = tokens;
    ctx->tokens_pos = 0;
    ctx->tokens_size = tokens_size;

    if (!json || len == 0 || !tokens || tokens_size < 8)
        ctx->status = CJ_ERROR;
    else
        ctx->status = CJ_OK;
}

void cj_parse(cj_ctx *ctx)
{
    int obj_depth;
    int arr_depth;
    unsigned pos;
    cj_token_ref tok_index;
    cj_token_ref tok_parent;
    cj_token *tok;
    const unsigned char *p;

    if (!ctx || ctx->status != CJ_OK)
        return;

    ctx->status = cj_is_valid_utf8(ctx->buf, ctx->size);
    if (ctx->status != CJ_OK)
        return;

    cj_trim_trailing_whitespace(ctx);

    obj_depth = 0;
    arr_depth = 0;
    pos = 0;
    p = ctx->buf;
    tok_parent = CJ_INVALID_TOKEN_INDEX;

    do
    {
        tok_index = cj_alloc_token(ctx, &tok);
        if (tok_index == CJ_INVALID_TOKEN_INDEX || tok == 0)
        {
            ctx->status = CJ_PARSE_TOKENS_EXHAUSTED;
            break;
        }

        pos = cj_next_token(p, ctx->size, pos, tok);

        if (tok->type == CJ_TOKEN_INVALID)
        {
            ctx->status = CJ_PARSE_INVALID_TOKEN;
            break;
        }

        tok->parent = tok_parent;
        CJ_ASSERT(tok->parent < (int)ctx->tokens_pos);

        if (tok->type == CJ_TOKEN_OBJECT_BEG)
        {
            obj_depth++;
            tok_parent = tok_index;
            CJ_ASSERT(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_BEG)
        {
            arr_depth++;
            tok_parent = tok_index;
            CJ_ASSERT(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_OBJECT_END)
        {
            obj_depth--;
            if (tok_index < 2 || obj_depth < 0)
            {
                break;
            }
            CJ_ASSERT(tok->parent >= 0);
            tok_parent = ctx->tokens[tok->parent].parent;
            tok->parent = tok_parent;
            CJ_ASSERT(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_END)
        {
            arr_depth--;
            if (tok_index < 2 || arr_depth < 0)
            {
                break;
            }
            CJ_ASSERT(tok->parent >= 0);
            tok_parent = ctx->tokens[tok->parent].parent;
            tok->parent = tok_parent;
            CJ_ASSERT(tok_parent < (int)ctx->tokens_pos);
        }

#if 0
        if (tok->type == CJ_TOKEN_STRING || tok->type == CJ_TOKEN_PRIMITIVE)
        {
            printf("JSON TOKEN[%d] (%c) pos: %u, len: %u, parent: %d, obj_depth: %d, arr_depth: %d, %.*s\n",
                tok_index, (char)tok->type, tok->pos, tok->len, tok->parent, obj_depth, arr_depth,
                tok->len, &ctx->buf[tok->pos]);
        }
        else
        {
            printf("JSON TOKEN[%d] (%c) pos: %u, len: %u, parent: %d, obj_depth: %d, arr_depth: %d\n",
                tok_index, (char)tok->type, tok->pos, tok->len, tok->parent, obj_depth, arr_depth);
        }
#endif

        if (tok->type == CJ_TOKEN_NAME_SEP)
        {
            if (tok_index < 2 || tok[-1].type != CJ_TOKEN_STRING)
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }
        }

        if (tok_index > 1)
        {
            if (tok[-1].type == CJ_TOKEN_NAME_SEP &&
                !(tok->type == CJ_TOKEN_STRING ||
                  tok->type == CJ_TOKEN_PRIMITIVE ||
                  tok->type == CJ_TOKEN_OBJECT_BEG ||
                  tok->type == CJ_TOKEN_ARRAY_BEG))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_ITEM_SEP &&
                !(tok->type == CJ_TOKEN_STRING ||
                  tok->type == CJ_TOKEN_PRIMITIVE ||
                  tok->type == CJ_TOKEN_OBJECT_BEG ||
                  tok->type == CJ_TOKEN_ARRAY_BEG))
            {
                pos = tok[-1].pos;
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_PRIMITIVE &&
                !(tok->type == CJ_TOKEN_ITEM_SEP ||
                  tok->type == CJ_TOKEN_OBJECT_END ||
                  tok->type == CJ_TOKEN_ARRAY_END))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }

            if (tok[-1].type == CJ_TOKEN_STRING &&
                !(tok->type == CJ_TOKEN_ITEM_SEP ||
                  tok->type == CJ_TOKEN_NAME_SEP ||
                  tok->type == CJ_TOKEN_OBJECT_END ||
                  tok->type == CJ_TOKEN_ARRAY_END))
            {
                ctx->status = CJ_PARSE_INVALID_TOKEN;
                break;
            }
        }
    }
    while (pos < ctx->size);

    ctx->pos = pos;

    if (ctx->status == CJ_OK && (obj_depth != 0 || arr_depth != 0))
        ctx->status = CJ_PARSE_PARENT_CLOSING;
}

cj_token_ref cj_value_ref(cj_ctx *ctx, cj_token_ref obj, const char *key)
{
    unsigned i;
    unsigned k;
    unsigned len;
    cj_token_ref result;
    cj_token *tok;

    result = CJ_INVALID_TOKEN_INDEX;

    if (!ctx || ctx->status != CJ_OK || !key)
        return result;

    if (obj < 0 || obj >= (cj_token_ref)ctx->tokens_pos)
        return result;

    for (len = 0; key[len]; len++)
    {}

    for (i = obj + 1; i < ctx->tokens_pos; i++)
    {
        tok = &ctx->tokens[i];
        if (tok->type != CJ_TOKEN_NAME_SEP)
            continue;

        tok = &tok[-1];

        if (tok->parent != obj)
            continue;

        if (tok->len != len)
            continue;

        for (k = 0; k < len; k++)
        {
            if (ctx->buf[tok->pos + k] != (unsigned char)key[k])
                break;
        }

        if (k == len)
        {
            result = i + 1;
            break;
        }
    }

    return result;
}

int cj_copy_value(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref obj, const char *key)
{
    unsigned i;
    cj_token_ref ref;
    cj_token *tok;

    buf[0] = '\0';
    ref = cj_value_ref(ctx, obj, key);

    if (ref != CJ_INVALID_TOKEN_INDEX)
    {
        tok = &ctx->tokens[ref];
        if (tok->len < size)
        {
            for (i = 0; i < tok->len; i++)
                buf[i] = ctx->buf[tok->pos + i];
            buf[tok->len] = '\0';
            return 1;
        }
    }

    return 0;
}

int cj_copy_ref(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref ref)
{
    unsigned i;
    cj_token *tok;

    buf[0] = '\0';

    if (ref != CJ_INVALID_TOKEN_INDEX)
    {
        tok = &ctx->tokens[ref];
        if (tok->len < size)
        {
            for (i = 0; i < tok->len; i++)
                buf[i] = ctx->buf[tok->pos + i];
            buf[tok->len] = '\0';
            return 1;
        }
    }

    return 0;
}
