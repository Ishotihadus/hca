#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <hca/decoder.h>
#include <hca/error.h>

typedef struct {
    bool available;
    uint32_t start;
    // The granule (block) to be decoded includes this index
    uint32_t end;
    uint16_t num_ignored_samples_first;
    // Meaning of this value is suspicious, but this is most reasonable
    uint16_t num_ignored_samples_last;
} HcaFileLoopInfo;

typedef struct {
    bool available;
    float value;
} HcaFileVolumeInfo;

typedef struct HcaFileInfo {
    uint16_t version;
    uint16_t header_size;
    uint8_t num_channels;
    uint32_t sampling_rate;
    uint32_t num_blocks;
    // These two values are known as soundless samples, but
    // should be the number of invalid samples.
    // Hence, the first `num_ignored_samples_first` samples and
    // the last `num_ignored_samples_last` ones should be ignored in decoding.
    // Since the first 128 samples are always invalid because of MDCT,
    // `num_ignored_samples_first` should be equal to or more than 128.
    uint16_t num_ignored_samples_first;
    uint16_t num_ignored_samples_last;
    uint32_t block_size;
    // 0: minimum resolution index of quantization (= 1)
    // 1: maximum resolution index of quantization (+1) (= 15)
    // 2: number of blocks of channels (>= 1, <= num_channels)
    // 3: parameter for configuring channel types
    // 4: total number of frequency bins
    // 5: number of frequency bins which are independent on each channel
    // 6: number of frequency bins which are common for stereo channels
    // 7: number of groups of frequency bins which used for high frequencies
    uint8_t compression_params[8];
    uint16_t ath_type;
    HcaFileLoopInfo loop;
    uint16_t encryption_type;
    HcaFileVolumeInfo volume;
    char *comment;
} HcaFileInfo;

typedef struct HcaFile {
    FILE *fp;
    HcaFileInfo info;
    HcaDecoder decoder;
    size_t current_index;
    long loop_start_file_pos;
    bool loop_started;
} HcaFile;

HcaError hca_file_info_init(HcaFileInfo *info, FILE *fp);
void hca_file_info_print(HcaFileInfo *info, FILE *fp);
int hca_file_info_get_num_samples(HcaFileInfo *info);
// the end sample is the "next" sample of the end of the loop
bool hca_file_info_get_loop(HcaFileInfo *info, int *start, int *end);
void hca_file_info_free(HcaFileInfo *info);

HcaError hca_file_init(HcaFile *file, FILE *fp, uint64_t key);
size_t hca_file_calc_buffer_size(HcaFile *file);
HcaError hca_file_read(double *buffer, size_t *written_size, bool loop, HcaFile *file);
void hca_file_free(HcaFile *file);
