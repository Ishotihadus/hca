#include <stdint.h>
#include <string.h>
#include <hca/bitreader.h>
#include <hca/endian_swap.h>

// For faster implementation, data must be at least 4 bytes longer than the real length.
void hca_bitreader_init(HcaBitReader *reader, void *data, int byte_length) {
    memset(data + byte_length, 0, 4);
    reader->data = data;
    reader->len = byte_length * 8;
    reader->pos = 0;
}

int hca_bitreader_read(HcaBitReader *reader, int bits) {
    static const uint16_t mask[] = { 0x0000,
        0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
        0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
    };
    if (reader->pos > reader->len)
        return 0;
    int pos = reader->pos;
    reader->pos += bits;
    void *d = reader->data + (pos / 8);
    return (bswapb_32(*(uint32_t*)d) >> (32 - pos % 8 - bits)) & mask[bits];
}

int hca_bitreader_check(HcaBitReader *reader, int bits) {
    static const uint16_t mask[] = { 0x0000,
        0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
        0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
    };
    if (reader->pos > reader->len)
        return 0;
    void *d = reader->data + (reader->pos / 8);
    return (bswapb_32(*(uint32_t*)d) >> (32 - reader->pos % 8 - bits)) & mask[bits];
}

void hca_bitreader_seek(HcaBitReader *reader, int bits) {
    reader->pos += bits;
}
