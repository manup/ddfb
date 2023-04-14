
/*
    +--first octet--+-second octet--+--third octet--+
    |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
    +-----------+---+-------+-------+---+-----------+
    |5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|5 4 3 2 1 0|
    +--1.index--+--2.index--+--3.index--+--4.index--+
*/

#define BASE64_INVALID_INDEX 64
#define BASE64_ERROR_INVALID_DATA -1
#define BASE64_ERROR_OUTBUF_TOO_SMALL -2

static const char *base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz"
                                     "0123456789"
                                     "+/";

static unsigned U_base64_char_index(char ch)
{
    unsigned result;
    const char *p;

    p = base64_alphabet;

    for (result = 0; *p; p++, result++)
        if (*p == ch)
            return result;
    return BASE64_INVALID_INDEX;
}



/* Returns number of bytes written in out on success,
   or a negative error value (BASE64_ERROR_*).

   'in' can optionally be '=' padded.
*/
int U_base64_decode(const char *in, unsigned inlen, unsigned char *out, unsigned outlen)
{
    int idx;
    int len;
    unsigned o;
    unsigned count;

    o = 0;
    count = 0;
    len = 0;
    while (inlen)
    {
        if (*in == '=')
        {
            if (inlen == 1 || (inlen == 2 && in[1] == '='))
                break;
        }

        idx = U_base64_char_index(*in++);
        if (idx == BASE64_INVALID_INDEX)
            return BASE64_ERROR_INVALID_DATA;
        o |= idx;
        count += 6;
        inlen--;

        if (count >= 8)
        {
            if (len == (int)outlen)
                return BASE64_ERROR_OUTBUF_TOO_SMALL;

            out[len++] = (unsigned char)(o >> (count - 8)) & 0xFF;
            o <<= 6;
            count -= 8;
            continue;
        }

        o <<= 6;
    }
    return len;
}

int U_base64_encode(const unsigned char *in, unsigned inlen, unsigned char *out, unsigned outlen)
{
    U_ASSERT(0 && "TODO implement");
    unsigned count;

    (void)in;
    (void)out;

    count = (inlen / 3) * 4;
    count += ((inlen % 3) != 0) ? 4 : 0;

    if (count < outlen)
        return BASE64_ERROR_OUTBUF_TOO_SMALL;

    return 0;
}


/*
    BASE64("") = ""
    BASE64("f") = "Zg=="
    BASE64("fo") = "Zm8="
    BASE64("foo") = "Zm9v"
    BASE64("foob") = "Zm9vYg=="
    BASE64("fooba") = "Zm9vYmE="
    BASE64("foobar") = "Zm9vYmFy"
*/
void base64_test(void)
{
    unsigned i;
    int ret;
    unsigned char out[64];
    const char *in[9] = { "", "Zg", "Zg==",  "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy", "Zm=vYmFy" };
    const int out_len[9] = {0, 1, 1, 2, 3, 4, 5, 6, -1};


    for (i = 0; i < U_ARRAY_SIZE(in); i++)
    {
        U_bzero(&out[0], sizeof(out));
        ret = U_base64_decode(in[i], U_strlen(in[i]), out, sizeof(out));
        U_Printf("base64: %s -> %s (%d vs %d)\n", in[i], (const char*)&out[0], ret, out_len[i]);
    }
}

