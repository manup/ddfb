#ifndef U_BSTREAM_H
#define U_BSTREAM_H

/* byte stream */

typedef enum
{
    U_BSTREAM_OK,
    U_BSTREAM_READ_PAST_END,
    U_BSTREAM_WRITE_PAST_END,
    U_BSTREAM_NOT_INITIALISED
} U_BStreamStatus;

typedef struct U_BStream
{
    u8 *data;
    unsigned pos;
    unsigned size;
    U_BStreamStatus status;
} U_BStream;

void U_bstream_init(U_BStream *bs, void *data, unsigned size);
void U_bstream_put_u8(U_BStream *bs, u8 v);
void U_bstream_put_u16_le(U_BStream *bs, u16 v);
void U_bstream_put_u32_le(U_BStream *bs, u32 v);
u8 U_bstream_get_u8(U_BStream *bs);
u16 U_bstream_get_u16_le(U_BStream *bs);
u16 U_bstream_get_u16_be(U_BStream *bs);
u32 U_bstream_get_u32_le(U_BStream *bs);
u32 U_bstream_get_u32_be(U_BStream *bs);

#endif /* U_BSTREAM_H */
