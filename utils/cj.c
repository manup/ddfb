static int cj_is_valid_utf8(const unsigned char *str, unsigned len)
{
    const char *s;
    unsigned codepoint;

    s = (const char*)str;
    if (len < 2)
        return CJ_INVALID_UTF8;

    /* test valid utf8 codepoints */
    for (; *s;)
    {
        s = U_utf8_codepoint(s, &codepoint);

        if (codepoint == 0)
            return CJ_INVALID_UTF8;
    }

    return CJ_OK;
}

static unsigned cj_eat_white_space(const unsigned char *str)
{
    unsigned result;

    result = 0;

    for (; str[result]; )
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

static void cj_set_status(cj_ctx *ctx, cj_status status)
{
    ctx->status = status;
    U_ASSERT(status == CJ_OK);
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
        *tok = NULL;
    }

    return result;
}

static unsigned cj_next_token(const unsigned char *str, unsigned len, unsigned pos, cj_token *tok)
{
    pos += cj_eat_white_space(&str[pos]);

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

void cj_parse_init(cj_ctx *ctx, char *json, unsigned len,
                         cj_token *tokens, unsigned tokens_size)
{
    ctx->buf = (unsigned char*)json;
    ctx->pos = 0;
    ctx->size = len;

    ctx->tokens = tokens;
    ctx->tokens_pos = 0;
    ctx->tokens_size = tokens_size;

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

    /* printf("parse JSON: '%s'\n", (const char*)ctx->buf); */

    obj_depth = 0;
    arr_depth = 0;
    pos = 0;
    p = ctx->buf;
    tok_parent = CJ_INVALID_TOKEN_INDEX;

    do
    {
        tok_index = cj_alloc_token(ctx, &tok);
        if (tok_index == CJ_INVALID_TOKEN_INDEX || tok == NULL)
        {
            cj_set_status(ctx, CJ_PARSE_TOKENS_EXHAUSTED);
            break;
        }

        pos = cj_next_token(p, ctx->size, pos, tok);

        if (tok->type == CJ_TOKEN_INVALID)
        {
            if (pos < ctx->size)
                cj_set_status(ctx, CJ_PARSE_INVALID_TOKEN);
            break;
        }

        tok->parent = tok_parent;
        assert(tok->parent < (int)ctx->tokens_pos);

        if (tok->type == CJ_TOKEN_OBJECT_BEG)
        {
            obj_depth++;
            tok_parent = tok_index;
            assert(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_BEG)
        {
            arr_depth++;
            tok_parent = tok_index;
            assert(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_OBJECT_END)
        {
            obj_depth--;
            if (tok_index < 2 || obj_depth < 0)
            {
                break;
            }
            assert(tok->parent >= 0);
            tok_parent = ctx->tokens[tok->parent].parent;
            assert(tok_parent < (int)ctx->tokens_pos);
        }
        else if (tok->type == CJ_TOKEN_ARRAY_END)
        {
            arr_depth--;
            if (tok_index < 2 || arr_depth < 0)
            {
                break;
            }
            assert(tok->parent >= 0);
            tok_parent = ctx->tokens[tok->parent].parent;
            assert(tok_parent < (int)ctx->tokens_pos);
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
                cj_set_status(ctx, CJ_PARSE_INVALID_TOKEN);
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
                cj_set_status(ctx, CJ_PARSE_INVALID_TOKEN);
                break;
            }

            if (tok[-1].type == CJ_TOKEN_ITEM_SEP &&
                !(tok->type == CJ_TOKEN_STRING ||
                  tok->type == CJ_TOKEN_PRIMITIVE ||
                  tok->type == CJ_TOKEN_OBJECT_BEG ||
                  tok->type == CJ_TOKEN_ARRAY_BEG))
            {
                pos = tok[-1].pos;
                cj_set_status(ctx, CJ_PARSE_INVALID_TOKEN);
                break;
            }

            if (tok[-1].type == CJ_TOKEN_PRIMITIVE &&
                !(tok->type == CJ_TOKEN_ITEM_SEP ||
                  tok->type == CJ_TOKEN_OBJECT_END ||
                  tok->type == CJ_TOKEN_ARRAY_END))
            {
                cj_set_status(ctx, CJ_PARSE_INVALID_TOKEN);
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
