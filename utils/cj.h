#ifndef _CJSON_H
#define _CJSON_H

#define CJ_INVALID_TOKEN_INDEX -1

typedef int cj_token_ref;

typedef enum cj_status
{
    CJ_OK = 0,
    CJ_ERROR = 1,
    CJ_INVALID_CODEPOINT = 2,
    CJ_INVALID_UTF8 = 3,
    CJ_BUFFER_TOO_SMALL = 4,
    CJ_PARSE_TOKENS_EXHAUSTED = 21,
    CJ_PARSE_PARENT_CLOSING   = 22,
    CJ_PARSE_INVALID_TOKEN    = 25
} cj_status;

typedef enum cj_token_type
{
    CJ_TOKEN_INVALID    = 'i',
    CJ_TOKEN_STRING     = 'S',
    CJ_TOKEN_PRIMITIVE  = 'P',
    CJ_TOKEN_ARRAY_BEG  = '[',
    CJ_TOKEN_ARRAY_END  = ']',
    CJ_TOKEN_OBJECT_BEG = '{',
    CJ_TOKEN_OBJECT_END = '}',
    CJ_TOKEN_ITEM_SEP   = ',',
    CJ_TOKEN_NAME_SEP   = ':'

} cj_token_type;


typedef struct cj_token
{
    cj_token_type type;
    unsigned pos;
    unsigned len;
    cj_token_ref parent;
} cj_token;

typedef struct cj_ctx
{
    unsigned char *buf;
    unsigned pos;
    unsigned size;

    /* parse */
    cj_token *tokens;
    unsigned tokens_pos;
    unsigned tokens_size;

    cj_status status;
} cj_ctx;

#ifdef __cplusplus
extern "C" {
#endif

void cj_parse_init(cj_ctx *ctx, char *json, unsigned len, cj_token *tokens, unsigned tokens_size);
void cj_parse(cj_ctx *ctx);
cj_token_ref cj_value_ref(cj_ctx *ctx, cj_token_ref obj, const char *key);
int cj_copy_value(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref obj, const char *key);
int cj_copy_ref(cj_ctx *ctx, char *buf, unsigned size, cj_token_ref ref);

#ifdef __cplusplus
}
#endif

#endif /* _CJSON_H */
