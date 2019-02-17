#pragma once

typedef struct HcaBitReader {
    void *data;
    int len;
    int pos;
} HcaBitReader;

void hca_bitreader_init(HcaBitReader *reader, void *data, int byte_length);
int hca_bitreader_read(HcaBitReader *reader, int bits);
int hca_bitreader_check(HcaBitReader *reader, int bits);
void hca_bitreader_seek(HcaBitReader *reader, int bits);
