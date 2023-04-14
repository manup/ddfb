
void U_bstream_init(U_BStream *bs, void *data, unsigned size)
{
    bs->data = (u8*)data;
    bs->pos = 0;
    bs->size = size;
    bs->status = U_BSTREAM_OK;
}

static int U_bstream_verify_write(U_BStream *bs, unsigned size)
{
    if (bs->status != U_BSTREAM_OK)
        return 0;

    if (!bs->data)
    {
        bs->status = U_BSTREAM_NOT_INITIALISED;
        return 0;
    }

    if ((bs->pos + size) > bs->size)
    {
        bs->status = U_BSTREAM_WRITE_PAST_END;
        return 0;
    }

    return 1;
}

static int U_bstream_verify_read(U_BStream *bs, unsigned size)
{
    if (bs->status != U_BSTREAM_OK)
        return 0;

    if (!bs->data)
    {
        bs->status = U_BSTREAM_NOT_INITIALISED;
        return 0;
    }

    if ((bs->pos + size) > bs->size)
    {
        bs->status = U_BSTREAM_READ_PAST_END;
        return 0;
    }

    return 1;
}

void U_bstream_put_u8(U_BStream *bs, u8 v)
{
    if (U_bstream_verify_write(bs, 1))
    {
        bs->data[bs->pos++] = v;
    }
}

void U_bstream_put_u16_le(U_BStream *bs, u16 v)
{
    if (U_bstream_verify_write(bs, 2))
    {
        bs->data[bs->pos++] = (v >> 0) & 0xFF;
        bs->data[bs->pos++] = (v >> 8) & 0xFF;
    }
}

void U_bstream_put_u32_le(U_BStream *bs, u32 v)
{
    if (U_bstream_verify_write(bs, 4))
    {
        bs->data[bs->pos++] = (v >> 0) & 0xFF;
        bs->data[bs->pos++] = (v >> 8) & 0xFF;
        bs->data[bs->pos++] = (v >> 16) & 0xFF;
        bs->data[bs->pos++] = (v >> 24) & 0xFF;
    }
}

u8 U_bstream_get_u8(U_BStream *bs)
{
    u8 result;
    result = 0;

    if (U_bstream_verify_read(bs, 1))
    {
        result = bs->data[bs->pos];
        bs->pos++;
    }

    return result;
}

u16 U_bstream_get_u16_le(U_BStream *bs)
{
    u16 result;
    result = 0;

    if (U_bstream_verify_read(bs, 2))
    {
        result = bs->data[bs->pos + 1];
        result <<= 8;
        result |= bs->data[bs->pos];
        bs->pos += 2;
    }

    return result;
}

u16 U_bstream_get_u16_be(U_BStream *bs)
{
    u16 result;
    result = 0;

    if (U_bstream_verify_read(bs, 2))
    {
        result = bs->data[bs->pos];
        result <<= 8;
        result |= bs->data[bs->pos + 1];
        bs->pos += 2;
    }

    return result;
}

u32 U_bstream_get_u32_le(U_BStream *bs)
{
    u32 result;
    result = 0;

    if (U_bstream_verify_read(bs, 4))
    {
        result = bs->data[bs->pos + 3];
        result <<= 8;
        result |= bs->data[bs->pos + 2];
        result <<= 8;
        result |= bs->data[bs->pos + 1];
        result <<= 8;
        result |= bs->data[bs->pos + 0];
        bs->pos += 4;
    }

    return result;
}

u32 U_bstream_get_u32_be(U_BStream *bs)
{
    u32 result;
    result = 0;

    if (U_bstream_verify_read(bs, 4))
    {
        result = bs->data[bs->pos];
        result <<= 8;
        result |= bs->data[bs->pos + 1];
        result <<= 8;
        result |= bs->data[bs->pos + 2];
        result <<= 8;
        result |= bs->data[bs->pos + 3];
        bs->pos += 4;
    }

    return result;
}
