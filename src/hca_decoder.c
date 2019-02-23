#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <hca/decoder.h>
#include <hca/file.h>
#include <hca/bitreader.h>
#include <hca/decryptor.h>
#include <hca/fft.h>
#include <hca/crc16.h>
#include <hca/error.h>

static const double zeros[2048] = {};

static double scaling_table[64];
static double scale_conversion_table[128] = {};

static int fft_ip[16];
static double fft_w[320];

void hca_decoder_prepare_table() {
    static bool initialized = false;
    if (initialized)
        return;
    for (int i = 0; i < 64; i++)
        scaling_table[i] = pow(2, (i - 63) * 53.0 / 128.0 + 3.5);
    for (int i = 2; i < 127; i++)
        scale_conversion_table[i] = pow(2, (i - 64) * 53.0 / 128.0);
    prepare_dct4(128, fft_ip, fft_w);
    initialized = true;
}

HcaError hca_decoder_init(HcaDecoder *decoder, HcaFileInfo *hca, uint64_t key) {
    static unsigned char ath_table[] = {
        0x78, 0x5F, 0x56, 0x51, 0x4E, 0x4C, 0x4B, 0x49, 0x48, 0x48, 0x47, 0x46, 0x46, 0x45, 0x45, 0x45,
        0x44, 0x44, 0x44, 0x44, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
        0x3F, 0x3F, 0x3F, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D,
        0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B,
        0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B,
        0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3F,
        0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
        0x3F, 0x3F, 0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43,
        0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x44, 0x44,
        0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x45,
        0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46,
        0x46, 0x46, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x48, 0x48, 0x48, 0x48,
        0x48, 0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x4A, 0x4A, 0x4A, 0x4A,
        0x4A, 0x4A, 0x4A, 0x4A, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C,
        0x4C, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4F, 0x4F, 0x4F,
        0x4F, 0x4F, 0x4F, 0x50, 0x50, 0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52,
        0x52, 0x52, 0x53, 0x53, 0x53, 0x53, 0x54, 0x54, 0x54, 0x54, 0x54, 0x55, 0x55, 0x55, 0x55, 0x56,
        0x56, 0x56, 0x56, 0x57, 0x57, 0x57, 0x57, 0x57, 0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x59, 0x5A,
        0x5A, 0x5A, 0x5A, 0x5B, 0x5B, 0x5B, 0x5B, 0x5C, 0x5C, 0x5C, 0x5D, 0x5D, 0x5D, 0x5D, 0x5E, 0x5E,
        0x5E, 0x5F, 0x5F, 0x5F, 0x60, 0x60, 0x60, 0x61, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62, 0x63, 0x63,
        0x63, 0x64, 0x64, 0x64, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x67, 0x68, 0x68, 0x68, 0x69,
        0x69, 0x6A, 0x6A, 0x6A, 0x6B, 0x6B, 0x6B, 0x6C, 0x6C, 0x6D, 0x6D, 0x6D, 0x6E, 0x6E, 0x6F, 0x6F,
        0x70, 0x70, 0x70, 0x71, 0x71, 0x72, 0x72, 0x73, 0x73, 0x73, 0x74, 0x74, 0x75, 0x75, 0x76, 0x76,
        0x77, 0x77, 0x78, 0x78, 0x78, 0x79, 0x79, 0x7A, 0x7A, 0x7B, 0x7B, 0x7C, 0x7C, 0x7D, 0x7D, 0x7E,
        0x7E, 0x7F, 0x7F, 0x80, 0x80, 0x81, 0x81, 0x82, 0x83, 0x83, 0x84, 0x84, 0x85, 0x85, 0x86, 0x86,
        0x87, 0x88, 0x88, 0x89, 0x89, 0x8A, 0x8A, 0x8B, 0x8C, 0x8C, 0x8D, 0x8D, 0x8E, 0x8F, 0x8F, 0x90,
        0x90, 0x91, 0x92, 0x92, 0x93, 0x94, 0x94, 0x95, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99, 0x99, 0x9A,
        0x9B, 0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0xA0, 0xA0, 0xA1, 0xA2, 0xA2, 0xA3, 0xA4, 0xA5, 0xA5,
        0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xAA, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAE, 0xAF, 0xB0, 0xB1, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD,
        0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD,
        0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xED, 0xEE,
        0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFF, 0xFF
    };

    decoder->file = hca;
    decoder->num_channels = decoder->file->num_channels;
    decoder->block_buffer = NULL;
    decoder->wave_buffer = NULL;

    if (hca->block_size == 0)
        return kHcaVBRNotSupported;
    else if (hca->block_size < 8)
        return kHcaInvalidBlockSize;
    if (hca->compression_params[0] != 1 || hca->compression_params[1] != 15)
        return kHcaInvalidCompressionParameter;

    decoder->decryptor.type = hca->encryption_type;
    decoder->decryptor.key = key;
    if (hca->encryption_type) {
        HcaError error = hca_decryptor_init_table(&decoder->decryptor);
        if (error)
            return error;
    }

    if (hca->ath_type == 1) {
        int i = 0;
        for (int v = 0; i < 128 && v < 0x51C000; i++, v += hca->sampling_rate)
            decoder->ath[i] = ath_table[v >> 13];
        memset(decoder->ath + i, 0xFF, 128 - i);
    } else if (hca->ath_type) {
        return kHcaInvalidAthType;
    } else {
        memset(decoder->ath, 0, 128);
    }

    if (hca->compression_params[6] == 0 || hca->num_channels <= hca->compression_params[2]) {
        for (int i = 0; i < hca->num_channels; i++)
            decoder->channels[i].type = 0;
    } else {
        switch (hca->compression_params[2] ? hca->num_channels / hca->compression_params[2] : hca->num_channels) {
            case 2:
                for (int i = 0; i < hca->num_channels; i += 2) {
                    decoder->channels[i].type = 1;
                    decoder->channels[i + 1].type = 2;
                }
                break;
            case 3:
                for (int i = 0; i < hca->num_channels; i += 3) {
                    decoder->channels[i].type = 1;
                    decoder->channels[i + 1].type = 2;
                    decoder->channels[i + 2].type = 0;
                }
                break;
            case 4:
                if (hca->compression_params[3]) {
                    for (int i = 0; i < hca->num_channels; i += 4) {
                        decoder->channels[i].type = 1;
                        decoder->channels[i + 1].type = 2;
                        decoder->channels[i + 2].type = 0;
                        decoder->channels[i + 3].type = 0;
                    }
                } else {
                    for (int i = 0; i < hca->num_channels; i += 2) {
                        decoder->channels[i].type = 1;
                        decoder->channels[i + 1].type = 2;
                    }
                }
                break;
            case 5:
                if (hca->compression_params[3] > 2) {
                    for (int i = 0; i < hca->num_channels; i += 5) {
                        decoder->channels[i].type = 1;
                        decoder->channels[i + 1].type = 2;
                        decoder->channels[i + 2].type = 0;
                        decoder->channels[i + 3].type = 0;
                        decoder->channels[i + 4].type = 0;
                    }
                } else {
                    for (int i = 0; i < hca->num_channels; i += 5) {
                        decoder->channels[i].type = 1;
                        decoder->channels[i + 1].type = 2;
                        decoder->channels[i + 2].type = 0;
                        decoder->channels[i + 3].type = 1;
                        decoder->channels[i + 4].type = 2;
                    }
                }
                break;
            case 6:
                for (int i = 0; i < hca->num_channels; i += 6) {
                    decoder->channels[i].type = 1;
                    decoder->channels[i + 1].type = 2;
                    decoder->channels[i + 2].type = 0;
                    decoder->channels[i + 3].type = 0;
                    decoder->channels[i + 4].type = 1;
                    decoder->channels[i + 5].type = 2;
                }
                break;
            case 7:
                for (int i = 0; i < hca->num_channels; i += 7) {
                    decoder->channels[i].type = 1;
                    decoder->channels[i + 1].type = 2;
                    decoder->channels[i + 2].type = 0;
                    decoder->channels[i + 3].type = 0;
                    decoder->channels[i + 4].type = 1;
                    decoder->channels[i + 5].type = 2;
                    decoder->channels[i + 6].type = 0;
                }
                break;
            case 8:
                for (int i = 0; i < hca->num_channels; i += 8) {
                    decoder->channels[i].type = 1;
                    decoder->channels[i + 1].type = 2;
                    decoder->channels[i + 2].type = 0;
                    decoder->channels[i + 3].type = 0;
                    decoder->channels[i + 4].type = 1;
                    decoder->channels[i + 5].type = 2;
                    decoder->channels[i + 6].type = 1;
                    decoder->channels[i + 7].type = 2;
                }
                break;
            default:
                return kHcaUnsupportedCompressionParameter;
        }
    }

    for (int i = 0; i < hca->num_channels; i++)
        decoder->channels[i].num_base_bins = decoder->channels[i].type == 2 ? hca->compression_params[5] : hca->compression_params[5] + hca->compression_params[6];

    decoder->value3_count = hca->compression_params[7] ?
        (hca->compression_params[4] - hca->compression_params[5] - hca->compression_params[6] + hca->compression_params[7] - 1) / hca->compression_params[7] : 0;

    hca_decoder_prepare_table();

    if ((decoder->block_buffer = (uint8_t*)malloc(hca->block_size + 2)) == NULL)
        return kHcaMemoryAllocationFailed;
    if ((decoder->wave_buffer = (double*)malloc(hca->num_channels * 1152 * sizeof(double))) == NULL)
        return kHcaMemoryAllocationFailed;
    memcpy(decoder->wave_buffer + 1024 * hca->num_channels, zeros, sizeof(double) * 128 * hca->num_channels);

    return kHcaSuccess;
}

HcaError hca_decoder_free(HcaDecoder *decoder) {
    free(decoder->block_buffer);
    free(decoder->wave_buffer);
    return kHcaSuccess;
}

static inline bool hca_decoder_zeroblock(HcaDecoder*);
void hca_decoder_decode_step1(HcaDecoder*, HcaBitReader*);
void hca_decoder_decode_step2(HcaDecoder*, HcaBitReader*);
void hca_decoder_decode_step3(HcaDecoder*);
void hca_decoder_decode_step4(HcaDecoder*);
void hca_decoder_decode_step5(HcaDecoder*);

HcaError hca_decoder_decode_block(HcaDecoder *decoder, FILE *fp) {
    HcaFileInfo *hca = decoder->file;
    if (fread(decoder->block_buffer, 1, hca->block_size, fp) != (size_t)hca->block_size)
        return kHcaEndOfFile;
    if (crc16(decoder->block_buffer, hca->block_size))
        return kHcaInvalidCheckSum;
    if (*(uint16_t*)decoder->block_buffer != 0xffff)
        return kHcaInvalidBlockSignature;

    // A block whose data are all zero means silence
    if (hca_decoder_zeroblock(decoder))
        return kHcaSuccess;

    hca_decryptor_decrypt(&decoder->decryptor, decoder->block_buffer + 2, hca->block_size - 4);

    HcaBitReader reader;
    hca_bitreader_init(&reader, decoder->block_buffer + 2, hca->block_size - 4);

    hca_decoder_decode_step1(decoder, &reader);
    hca_decoder_decode_step2(decoder, &reader);
    hca_decoder_decode_step3(decoder);
    hca_decoder_decode_step4(decoder);
    hca_decoder_decode_step5(decoder);
    return kHcaSuccess;
}

static inline bool hca_decoder_zeroblock(HcaDecoder *decoder) {
    int i, size = decoder->file->block_size - 4;
    uint8_t *block = decoder->block_buffer + 2;
    for (i = 0; i < size - 7; i += 8)
        if (*(uint64_t*)(block + i) != 0)
            return false;
    for (; i < size; i++)
        if (block[i] != 0)
            return false;
    memcpy(decoder->wave_buffer, decoder->wave_buffer + decoder->num_channels * 1024, sizeof(double) * decoder->num_channels * 128);
    memcpy(decoder->wave_buffer + decoder->num_channels * 128, zeros, sizeof(double) * decoder->num_channels * 1024);
    return true;
}

void hca_decoder_decode_step1(HcaDecoder *decoder, HcaBitReader *reader) {
    static const int invert_table[] = {
        14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12, 11,
        11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10,  9,  9,  9,  9,  9,  9,  8,
         8,  8,  8,  8,  8,  7,  6,  6,  5,  4,  4,  4,  3,  3,  3,  2,  2,  2,  2
    };
    static const double range_table[] = {
        0, 2.0 / 3, 2.0 / 5, 2.0 / 7, 2.0 / 9, 2.0 / 11, 2.0 / 13, 2.0 / 15,
        2.0 / 31, 2.0 / 63, 2.0 / 127, 2.0 / 255, 2.0 / 511, 2.0 / 1023, 2.0 / 2047, 2.0 / 4095
    };

    int range_mod_table[128];
    int range_mod_base = hca_bitreader_read(reader, 9);
    int range_mod_border = hca_bitreader_read(reader, 7);
    if (decoder->file->ath_type) {
        int i = 0;
        for (; i < range_mod_border; i++)
            range_mod_table[i] = decoder->ath[i] + range_mod_base;
        ++range_mod_base;
        for (; i < 128; i++)
            range_mod_table[i] = decoder->ath[i] + range_mod_base;
    } else {
        int i = 0;
        for (; i < range_mod_border; i++)
            range_mod_table[i] = range_mod_base;
        ++range_mod_base;
        for (; i < 128; i++)
            range_mod_table[i] = range_mod_base;
    }

    for (int c = 0; c < decoder->num_channels; c++) {
        HcaDecoderChannelData *channel = decoder->channels + c;

        const int v = hca_bitreader_read(reader, 3);

        if (v) {
            if (v >= 6) {
                for (int i = 0; i < channel->num_base_bins; i++)
                    channel->range[i] = hca_bitreader_read(reader, 6);
            } else {
                channel->range[0] = hca_bitreader_read(reader, 6);
                for (int i = 1, v1 = (1 << v) - 1, v2 = v1 >> 1; i < channel->num_base_bins; i++) {
                    int v3 = hca_bitreader_read(reader, v);
                    channel->range[i] = v1 == v3 ? hca_bitreader_read(reader, 6) : (channel->range[i - 1] + v3 - v2) & 63;
                }
            }
            if (channel->type == 2)
                for (int i = 0; i < 8; i++)
                    channel->stereo_intensity_ratio[i] = hca_bitreader_read(reader, 4);
            else
                for (int i = 0, k = channel->num_base_bins, l = channel->num_base_bins - 1; i < decoder->value3_count; i++)
                    for (int j = 0, v1 = hca_bitreader_read(reader, 6) + 64; j < decoder->file->compression_params[7] && k < decoder->file->compression_params[4]; j++, k++, l--)
                        channel->high_frequency_scale[k] = scale_conversion_table[v1 - channel->range[l]];
            for (int i = 0; i < channel->num_base_bins; i++) {
                int scale = channel->range[i];
                if (channel->range[i]) {
                    channel->range[i] = range_mod_table[i] - (channel->range[i] * 5) / 2;
                    channel->range[i] = channel->range[i] < 0 ? 15 : channel->range[i] < 57 ? invert_table[channel->range[i]] : 1;
                }
                channel->gain[i] = scaling_table[scale] * range_table[channel->range[i]];
            }
        } else {
            memset(channel->range, 0, sizeof(int) * 128);
            memcpy(channel->gain, zeros, sizeof(double) * 128);
            if (channel->type == 2)
                for (int i = 0; i < 8; i++)
                    channel->stereo_intensity_ratio[i] = hca_bitreader_read(reader, 4);
            else if (channel->type != 2)
                hca_bitreader_seek(reader, 6 * decoder->value3_count);
        }
    }
}

void hca_decoder_decode_step2(HcaDecoder *decoder, HcaBitReader *reader) {
    static const int max_bit_table[] = { 0, 2, 3, 3, 4, 4, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0 };
    static const int read_bit_table[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        2, 2, 2, 2, 2, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
        2, 2, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
        3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    };
    static const double read_val_table[] = {
        0, 0, 0,  0,  0,  0, 0,  0,  0,  0, 0,  0,  0,  0, 0,  0,
        0, 0, 1, -1,  0,  0, 0,  0,  0,  0, 0,  0,  0,  0, 0,  0,
        0, 0, 1,  1, -1, -1, 2, -2,  0,  0, 0,  0,  0,  0, 0,  0,
        0, 0, 1, -1,  2, -2, 3, -3,  0,  0, 0,  0,  0,  0, 0,  0,
        0, 0, 1,  1, -1, -1, 2,  2, -2, -2, 3,  3, -3, -3, 4, -4,
        0, 0, 1,  1, -1, -1, 2,  2, -2, -2, 3, -3,  4, -4, 5, -5,
        0, 0, 1,  1, -1, -1, 2, -2,  3, -3, 4, -4,  5, -5, 6, -6,
        0, 0, 1, -1,  2, -2, 3, -3,  4, -4, 5, -5,  6, -6, 7, -7
    };

    for (int n = 0; n < 8; n++) {
        for (int c = 0; c < decoder->num_channels; c++) {
            HcaDecoderChannelData *channel = decoder->channels + c;
            memcpy(channel->block[n], zeros, sizeof(double) * 128);
            for (int i = 0; i < channel->num_base_bins; i++) {
                int r = channel->range[i];
                int bit_size = max_bit_table[r];
                if (r < 8) {
                    int v = hca_bitreader_check(reader, bit_size) + (r << 4);
                    channel->block[n][i] = channel->gain[i] * read_val_table[v];
                    hca_bitreader_seek(reader, read_bit_table[v]);
                } else {
                    int v = hca_bitreader_read(reader, bit_size - 1);
                    channel->block[n][i] = v ? channel->gain[i] * (hca_bitreader_read(reader, 1) ? -v : v) : 0;
                }
            }
        }
    }
}

void hca_decoder_decode_step3(HcaDecoder *decoder) {
    if (decoder->file->version < 0x200 || decoder->file->compression_params[7] == 0)
        return;
    for (int c = 0; c < decoder->num_channels; c++) {
        HcaDecoderChannelData *channel = decoder->channels + c;
        if (channel->type == 2)
            continue;
        for (int n = 0; n < 8; n++) {
            for (int k = channel->num_base_bins, l = channel->num_base_bins - 1; k < decoder->file->compression_params[4]; k++, l--)
                channel->block[n][k] = channel->high_frequency_scale[k] * channel->block[n][l];
            channel->block[n][127] = 0;
        }
    }
}

void hca_decoder_decode_step4(HcaDecoder *decoder) {
    static const double intensity_ratio_table1[] = {
        2, 13 / 7.0, 12 / 7.0, 11 / 7.0, 10 / 7.0, 9 / 7.0, 8 / 7.0,
        1,  6 / 7.0,  5 / 7.0,  4 / 7.0,  3 / 7.0, 2 / 7.0, 1 / 7.0, 0, 0
    };
    static const double intensity_ratio_table2[] = {
        0, 1 / 7.0, 2 / 7.0,  3 / 7.0,  4 / 7.0,  5 / 7.0,  6 / 7.0,
        1, 8 / 7.0, 9 / 7.0, 10 / 7.0, 11 / 7.0, 12 / 7.0, 13 / 7.0, 2, 2
    };

    if (decoder->file->compression_params[6] == 0)
        return;
    for (int c = 0; c < decoder->num_channels - 1; c++) {
        if (decoder->channels[c].type != 1)
            continue;
        for (int n = 0; n < 8; n++) {
            double f1 = intensity_ratio_table1[decoder->channels[c + 1].stereo_intensity_ratio[n]];
            double f2 = intensity_ratio_table2[decoder->channels[c + 1].stereo_intensity_ratio[n]];
            for (int i = decoder->file->compression_params[5]; i < decoder->file->compression_params[4]; i++) {
                decoder->channels[c + 1].block[n][i] = decoder->channels[c].block[n][i] * f2;
                decoder->channels[c].block[n][i] *= f1;
            }
        }
    }
}

void hca_decoder_decode_step5(HcaDecoder *decoder) {
    static const double window[] = {
        -6.90533780e-04f / 8, -1.97623484e-03f / 8, -3.67386453e-03f / 8, -5.72424009e-03f / 8,
        -8.09670333e-03f / 8, -1.07731819e-02f / 8, -1.37425177e-02f / 8, -1.69978570e-02f / 8,
        -2.05352642e-02f / 8, -2.43529025e-02f / 8, -2.84505188e-02f / 8, -3.28290947e-02f / 8,
        -3.74906212e-02f / 8, -4.24378961e-02f / 8, -4.76744287e-02f / 8, -5.32043017e-02f / 8,
        -5.90321124e-02f / 8, -6.51628822e-02f / 8, -7.16020092e-02f / 8, -7.83552229e-02f / 8,
        -8.54284912e-02f / 8, -9.28280205e-02f / 8, -1.00560151e-01f / 8, -1.08631350e-01f / 8,
        -1.17048122e-01f / 8, -1.25816986e-01f / 8, -1.34944350e-01f / 8, -1.44436508e-01f / 8,
        -1.54299513e-01f / 8, -1.64539129e-01f / 8, -1.75160721e-01f / 8, -1.86169162e-01f / 8,
        -1.97568730e-01f / 8, -2.09362969e-01f / 8, -2.21554622e-01f / 8, -2.34145418e-01f / 8,
        -2.47135997e-01f / 8, -2.60525763e-01f / 8, -2.74312705e-01f / 8, -2.88493186e-01f / 8,
        -3.03061932e-01f / 8, -3.18011731e-01f / 8, -3.33333343e-01f / 8, -3.49015296e-01f / 8,
        -3.65043819e-01f / 8, -3.81402701e-01f / 8, -3.98073107e-01f / 8, -4.15033519e-01f / 8,
        -4.32259798e-01f / 8, -4.49725032e-01f / 8, -4.67399567e-01f / 8, -4.85251158e-01f / 8,
        -5.03244936e-01f / 8, -5.21343827e-01f / 8, -5.39508522e-01f / 8, -5.57697773e-01f / 8,
        -5.75868905e-01f / 8, -5.93978047e-01f / 8, -6.11980557e-01f / 8, -6.29831433e-01f / 8,
        -6.47486031e-01f / 8, -6.64900243e-01f / 8, -6.82031155e-01f / 8, -6.98837578e-01f / 8,
        -7.15280414e-01f / 8, -7.31323123e-01f / 8, -7.46932149e-01f / 8, -7.62077332e-01f / 8,
        -7.76731849e-01f / 8, -7.90872812e-01f / 8, -8.04481268e-01f / 8, -8.17542017e-01f / 8,
        -8.30044091e-01f / 8, -8.41980159e-01f / 8, -8.53346705e-01f / 8, -8.64143789e-01f / 8,
        -8.74374807e-01f / 8, -8.84046197e-01f / 8, -8.93167078e-01f / 8, -9.01749134e-01f / 8,
        -9.09806132e-01f / 8, -9.17353690e-01f / 8, -9.24408972e-01f / 8, -9.30990338e-01f / 8,
        -9.37117040e-01f / 8, -9.42809045e-01f / 8, -9.48086798e-01f / 8, -9.52970862e-01f / 8,
        -9.57481921e-01f / 8, -9.61640537e-01f / 8, -9.65466917e-01f / 8, -9.68980789e-01f / 8,
        -9.72201586e-01f / 8, -9.75147963e-01f / 8, -9.77837980e-01f / 8, -9.80289042e-01f / 8,
        -9.82517719e-01f / 8, -9.84539866e-01f / 8, -9.86370564e-01f / 8, -9.88024116e-01f / 8,
        -9.89514053e-01f / 8, -9.90853190e-01f / 8, -9.92053449e-01f / 8, -9.93126273e-01f / 8,
        -9.94082093e-01f / 8, -9.94930983e-01f / 8, -9.95682180e-01f / 8, -9.96344328e-01f / 8,
        -9.96925533e-01f / 8, -9.97433305e-01f / 8, -9.97874618e-01f / 8, -9.98256087e-01f / 8,
        -9.98583674e-01f / 8, -9.98862922e-01f / 8, -9.99099135e-01f / 8, -9.99296963e-01f / 8,
        -9.99460995e-01f / 8, -9.99595225e-01f / 8, -9.99703407e-01f / 8, -9.99789119e-01f / 8,
        -9.99855518e-01f / 8, -9.99905586e-01f / 8, -9.99941945e-01f / 8, -9.99967217e-01f / 8,
        -9.99983609e-01f / 8, -9.99993265e-01f / 8, -9.99998033e-01f / 8, -9.99999762e-01f / 8
    };

    double t[256];
    int nc = decoder->num_channels;
    memcpy(decoder->wave_buffer, decoder->wave_buffer + 1024 * decoder->num_channels, sizeof(double) * 128 * nc);
    for (int c = 0; c < nc; c++) {
        for (int n = 0; n < 8; n++) {
            double *block = decoder->channels[c].block[n];
            dct4(128, block, t, fft_ip, fft_w);
            int iw = n * nc * 128 + c, ib = 64, j = 0;
            for (; j < 64; j++, iw += nc, ib++)
                decoder->wave_buffer[iw] -= window[j] * block[ib];
            for (ib--; j < 128; j++, iw += nc, ib--)
                decoder->wave_buffer[iw] += window[j] * block[ib];
            for (j--; j >= 64; j--, iw += nc, ib--)
                decoder->wave_buffer[iw] = window[j] * block[ib];
            for (ib++; j >= 0; j--, iw += nc, ib++)
                decoder->wave_buffer[iw] = window[j] * block[ib];
        }
    }
}
