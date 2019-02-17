#pragma once
#include <stdint.h>
#include <hca/decryptor.h>

typedef struct {
    int type;
    int value1_count;
    int value1[128];
    int value2[8];
    int *value3;
    int scale[128];
    double base[128];
    double block[8][128];
} HcaDecoderChannelData;

typedef struct HcaDecoder {
    struct HcaFileInfo *file;
    int num_channels;
    uint8_t ath[128];
    HcaDecryptor decryptor;
    int value3_count;
    HcaDecoderChannelData channels[16];
    uint8_t *block_buffer;
    double *wave_buffer;
} HcaDecoder;

enum HcaError hca_decoder_init(HcaDecoder *decoder, struct HcaFileInfo *info, uint64_t key);
enum HcaError hca_decoder_free(HcaDecoder *decoder);
enum HcaError hca_decoder_decode_block(HcaDecoder *decoder, FILE *fp);
