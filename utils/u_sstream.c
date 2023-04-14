
void U_sstream_init(U_SStream *ss, void *str, unsigned size)
{
    ss->str = (char*)str;
    ss->pos = 0;
    ss->len = size;
}

const char *U_sstream_str(const U_SStream *ss)
{
    U_ASSERT(ss->pos < ss->len);
    return &ss->str[ss->pos];
}


unsigned U_sstream_pos(const U_SStream *ss)
{
    return ss->pos;
}

unsigned U_sstream_remaining(const U_SStream *ss)
{
    U_ASSERT (ss->pos <= ss->len);
    return ss->len - ss->pos;
}

int U_sstream_at_end(const U_SStream *ss)
{
    U_ASSERT (ss->pos <= ss->len);
    return (ss->len - ss->pos) == 0;
}

int U_sstream_get_i32(U_SStream *ss, int base)
{
    long r;
    char *nptr;
    char *endptr;

    r = 0;

    if (ss->pos < ss->len)
    {
        errno = 0;
        nptr = &ss->str[ss->pos];
        endptr = NULL;

        r = strtol(nptr, &endptr, base);
        if (errno != 0)
            r = 0;

        if (endptr)
            ss->pos += (unsigned)(endptr - nptr);
    }

    return (int)r;
}

float U_sstream_get_f32(U_SStream *ss)
{
    float r;
    char *nptr;
    char *endptr;

    r = 0.0f;

    if (ss->pos < ss->len)
    {
        errno = 0;
        nptr = &ss->str[ss->pos];
        endptr = NULL;

        r = strtof(nptr, &endptr);
        if (errno != 0)
            r = 0.0f;
    }

    return r;
}

double U_sstream_get_f64(U_SStream *ss)
{
    double r;
    char *nptr;
    char *endptr;

    r = 0.0f;

    if (ss->pos < ss->len)
    {
        errno = 0;
        nptr = &ss->str[ss->pos];
        endptr = NULL;

        r = strtod(nptr, &endptr);
        if (errno != 0)
            r = 0.0f;
    }

    return r;
}

char U_sstream_peek_char(U_SStream *ss)
{
    if (ss->pos < ss->len)
        return ss->str[ss->pos];
    return 0;
}

void U_sstream_skip_whitespace(U_SStream *ss)
{
    char ch;
    while (ss->pos < ss->len)
    {
        ch = ss->str[ss->pos];
        switch (ch)
        {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                ss->pos++;
                break;
            default:
                return;
        }
    }
}

const char *U_sstream_next_token(U_SStream *ss, const char *delim)
{
    const char *d;
    const char *result;

    result = &ss->str[ss->pos]; /* save current pos */

    /* skip over until we see a delimeter */
    for (; ss->pos < ss->len; ss->pos++)
    {
        d = delim;

        for (; *d; d++)
        {
            if (ss->str[ss->pos] == *d)
            {
                ss->str[ss->pos] = '\0'; /* mark end of current token */
                ss->pos++;
                goto skip_delim;
            }
        }
    }

    return result;

skip_delim:
    for (; ss->pos < ss->len; ss->pos++)
    {
        d = delim;

        for (; *d; d++)
        {
            if (ss->str[ss->pos] == *d)
            {
                ss->pos++;
                goto skip_delim;
            }
        }

        break;
    }

    return result;
}

int U_sstream_starts_with(U_SStream *ss, const char *str)
{
    unsigned len;

    len = U_strlen(str);

    if ((ss->len - ss->pos) >= len)
    {
        return U_memcmp(&ss->str[ss->pos], str, len) == 0 ? 1 : 0;
    }

    return 0;
}

void U_sstream_put_str(U_SStream *ss, const char *str)
{
    unsigned len;

    len = U_strlen(str);

    U_ASSERT(ss->pos + len + 1 < ss->len);
    if (ss->pos + len + 1 < ss->len)
    {
        U_memcpy(&ss->str[ss->pos], str, len);
        ss->pos += len;
        ss->str[ss->pos] = '\0';
    }
}

/*  Outputs the signed 32-bit integer 'num' as ASCII string.
    `num`: -2147483648 .. 2147483647
*/
void U_sstream_put_i32(U_SStream *ss, long int num)
{
    int i;
    int pos;
    int remainder;
    long int n;
    unsigned char buf[16];

    /* sign + max digits + NUL := 12 bytes */

    n = num;
    if (n < 0)
    {
        ss->str[ss->pos++] = '-';
        n = -n;
    }

    pos = 0;
    do
    {
        remainder = n % 10;
        n = n / 10;
        buf[pos++] = '0' + remainder;
    }
    while (n);

    if (ss->len - ss->pos < (unsigned)pos + 1) /* not enough space */
        return;

    for (i = pos; i > 0; i--) /* reverse copy */
    {
        ss->str[ss->pos++] = buf[i - 1];
    }

    ss->str[ss->pos] = '\0';
}

void U_sstream_put_u32(U_SStream *ss, unsigned long num)
{
    U_ASSERT(num <= 2147483647);
    /* TODO proper implementation */
    U_sstream_put_i32(ss, (long int)num);
}

static const char _hex_table[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

void U_sstream_put_hex(U_SStream *ss, const void *data, unsigned size)
{
    unsigned i;
    const u8 *buf;
    u8 nib;

    if ((ss->len - ss->pos) < (size * 2) + 1)
        return;

    buf = (const u8*)data;

    for (i = 0; i < size; i++)
    {
        nib = buf[i];
        ss->str[ss->pos] = _hex_table[(nib & 0xF0) >> 4];
        ss->pos++;
        ss->str[ss->pos] = _hex_table[(nib & 0x0F)];
        ss->pos++;
    }

    ss->str[ss->pos] = '\0';
}

void U_sstream_seek(U_SStream *ss, unsigned pos)
{
    if (pos <= ss->len)
        ss->pos = pos;
}
