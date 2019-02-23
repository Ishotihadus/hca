#pragma once
#include <stdint.h>
#include <hca/decryptor.h>
#include <hca/error.h>

typedef struct {
    int type;
    int num_base_bins;
    int range[128];
    int stereo_intensity_ratio[8];
    double gain[128];
    double high_frequency_scale[128];
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

HcaError hca_decoder_init(HcaDecoder *decoder, struct HcaFileInfo *info, uint64_t key);
HcaError hca_decoder_free(HcaDecoder *decoder);
HcaError hca_decoder_decode_block(HcaDecoder *decoder, FILE *fp);
