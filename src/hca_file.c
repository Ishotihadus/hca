#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <hca/file.h>
#include <hca/decoder.h>
#include <hca/crc16.h>
#include <hca/error.h>
#include <hca/endian_swap.h>

#define READ(x) if (fread(buffer, 1, (x), hfp) < (x)) { error = kHcaFileSizeNotSufficient; goto finalize; }
#define READTO(y, x) if (fread((y), 1, (x), hfp) < (x)) { error = kHcaFileSizeNotSufficient; goto finalize; }
#define READ8(x)  if (!fread(&(x), 1, 1, hfp)) { error = kHcaFileSizeNotSufficient; goto finalize; }
#define READ16(x) if (!fread(&(x), 2, 1, hfp)) { error = kHcaFileSizeNotSufficient; goto finalize; } (x) = bswapb_16(x);
#define READ32(x) if (!fread(&(x), 4, 1, hfp)) { error = kHcaFileSizeNotSufficient; goto finalize; } (x) = bswapb_32(x);
#define READ16F(x) if (!fread(&float_buffer, 2, 1, hfp)) { error = kHcaFileSizeNotSufficient; goto finalize; } float_buffer.i = bswapb_16(float_buffer.i); (x) = float_buffer.f;

HcaError hca_file_info_init(HcaFileInfo *info, FILE *fp) {
    enum  {
        kHeaderHCA = 1,
        kHeaderFMT = 2,
        kHeaderCOMP = 4,
        kHeaderDEC = 4,
        kHeaderFulfill = 7
    } chunk_fulfillments = 0;

    info->loop.available = false;
    info->encryption_type = 0;
    info->volume.available = false;
    info->comment = NULL;

    uint32_t hca_chunk[2];
    if (fread(hca_chunk, 4, 2, fp) < 2)
        return kHcaFileSizeNotSufficient;
    if ((bswapb_32(hca_chunk[0]) & 0x7f7f7f7f) != 0x48434100)
        return kHcaInvalidFileSignature;
    uint32_t hca_chunk_tmp = bswapb_32(hca_chunk[1]);
    info->version = hca_chunk_tmp >> 16;
    info->header_size = hca_chunk_tmp & 0xffff;
    info->ath_type = info->version < 0x200;
    chunk_fulfillments |= kHeaderHCA;

    char *header = (char*)malloc(info->header_size);
    if (!header)
        return kHcaMemoryAllocationFailed;

    memcpy(header, hca_chunk, 8);
    if (fread(header + 8, 1, info->header_size - 8, fp) < info->header_size - 8) {
        free(header);
        return kHcaFileSizeNotSufficient;
    }
    if (crc16(header, info->header_size)) {
        free(header);
        return kHcaInvalidCheckSum;
    }

    HcaError error = kHcaSuccess;
    FILE *hfp = fmemopen(header + 8, info->header_size - 8, "rb");

    // 読み込み用バッファ類
    uint32_t chunk_name;
    unsigned char buffer[16];
    union {
        float f;
        uint16_t i;
    } float_buffer;

    while (1) {
        if (!fread(&chunk_name, 4, 1, hfp))
            break;
        switch(bswapb_32(chunk_name) & 0x7f7f7f7f) {
            case 0x666D7400: // fmt
                READ32(info->sampling_rate);
                info->num_channels = info->sampling_rate >> 24;
                info->sampling_rate &= 0xffffff;
                READ32(info->num_blocks);
                READ16(info->num_ignored_samples_first);
                READ16(info->num_ignored_samples_last);
                chunk_fulfillments |= kHeaderFMT;
                break;
            case 0x636F6D70: // comp
                READ16(info->block_size);
                READTO(info->compression_params, 8);
                READ(2);
                chunk_fulfillments |= kHeaderCOMP;
                break;
            case 0x64656300: // dec
                READ16(info->block_size);
                READ(6);
                info->compression_params[0] = buffer[0];
                info->compression_params[1] = buffer[1];
                info->compression_params[2] = buffer[4] & 0xf;
                info->compression_params[3] = buffer[4] >> 4;
                info->compression_params[4] = buffer[2] + 1;
                info->compression_params[5] = (buffer[5] ? buffer[3] : buffer[2]) + 1;
                info->compression_params[6] = buffer[5] ? buffer[2] - buffer[3] : 0;
                info->compression_params[7] = 0;
                chunk_fulfillments |= kHeaderDEC;
                break;
            case 0x76627200: // vbr
                READ(4);
                break;
            case 0x61746800: // ath
                READ16(info->ath_type);
                break;
            case 0x6C6F6F70: // loop
                info->loop.available = true;
                READ32(info->loop.start);
                READ32(info->loop.end);
                READ16(info->loop.num_ignored_samples_first);
                READ16(info->loop.num_ignored_samples_last);
                break;
            case 0x63697068: // ciph
                READ16(info->encryption_type);
                break;
            case 0x72766100: // rva
                info->volume.available = true;
                READ16F(info->volume.value);
                break;
            case 0x636F6D6D: // comm
                READ(1);
                info->comment = (char*)malloc(buffer[0] + 1);
                if (info->comment == NULL) {
                    error = kHcaMemoryAllocationFailed;
                    goto finalize;
                }
                READTO(info->comment, buffer[0]);
                info->comment[buffer[0]] = 0;
                break;
            case 0x70616400: // pad
                break;
            case 0x00000000:
                goto end_header_loop;
            default:
                error = kHcaUnknownChunk;
                goto finalize;
        }
    }
    end_header_loop:

    if (chunk_fulfillments != kHeaderFulfill)
        error = kHcaHeaderNotSufficient;

    finalize:

    fclose(hfp);
    free(header);
    return error;
}

void hca_file_info_print(HcaFileInfo *info, FILE *fp) {
    fprintf(fp, "HCA Version    : %x.%02x\n", info->version >> 8, info->version & 0xff);
    fprintf(fp, "Channels       : %d\n", info->num_channels);
    fprintf(fp, "Sample Rate    : %d\n", info->sampling_rate);
    size_t num_samples = hca_file_info_get_num_samples(info);
    double seconds = num_samples * 1.0 / info->sampling_rate;
    int mins = seconds / 60;
    seconds -= mins * 60;
    int hours = mins / 60;
    mins -= hours * 60;
    fprintf(fp, "Duration       : %02d:%02d:%05.2f = %zu samples\n", hours, mins, seconds, num_samples);
    if (info->block_size == 0)
        fprintf(fp, "Bit Rate       : VBR\n");
    else
        fprintf(fp, "Bit Rate       : %d kbps\n", info->block_size * info->sampling_rate / 131072);
    fprintf(fp, "Enryption      : %d (%s)\n", info->encryption_type,
        info->encryption_type == 0 ? "no encryption" : info->encryption_type == 1 ? "encrypted with common table" :
        info->encryption_type == 56 ? "encrypted with a custom key" : "unknown");
    fprintf(fp, "Loop           : %s\n", info->loop.available ? "enabled" : "disabled");
    if (info->volume.available)
        fprintf(fp, "Relative Volume: %f\n", info->volume.value);
    else
        fprintf(fp, "Relative Volume: disabled\n");
}

int hca_file_info_get_num_samples(HcaFileInfo *info) {
    return info->num_blocks * 1024 - info->num_ignored_samples_first - info->num_ignored_samples_last;
}

bool hca_file_info_get_loop(HcaFileInfo *info, int *start, int *end) {
    if (!info->loop.available)
        return false;
    *start = info->loop.start * 1024 + info->loop.num_ignored_samples_first;
    *end = (info->loop.end + 1) * 1024 - info->loop.num_ignored_samples_last;
    return true;
}

void hca_file_info_free(HcaFileInfo *info) {
    free(info->comment);
}

HcaError hca_file_init(HcaFile *file, FILE *fp, uint64_t key) {
    HcaError error;
    file->fp = fp;
    file->current_index = 0;
    if ((error = hca_file_info_init(&file->info, fp)))
        return error;
    if (file->info.num_blocks * 1024 < file->info.num_ignored_samples_first + file->info.num_ignored_samples_last) {
        hca_file_info_free(&file->info);
        return kHcaInvalidCompressionParameter;
    }
    if ((error = hca_decoder_init(&file->decoder, &file->info, key))) {
        hca_file_info_free(&file->info);
        return error;
    }
    return kHcaSuccess;
}

size_t hca_file_calc_buffer_size(HcaFile *file) {
    return sizeof(double) * 1024 * file->info.num_channels;
}

HcaError hca_file_read(HcaFile *file, double *buffer, size_t *num_samples) {
    *num_samples = 0;
    if (file->current_index >= file->info.num_blocks - file->info.num_ignored_samples_last / 1024)
        return kHcaEndOfFile;
    HcaError error = hca_decoder_decode_block(&file->decoder, file->fp);
    ++file->current_index;
    if (error)
        return error == kHcaEndOfFile ? kHcaFileSizeNotSufficient : error;
    double *wave_ptr = file->decoder.wave_buffer;
    int length = 1024;
    int skip = file->info.num_ignored_samples_first - (file->current_index - 1) * 1024;
    if (skip >= 1024) {
        return hca_file_read(file, buffer, num_samples);
    } else if (skip > 0) {
        wave_ptr += skip * file->info.num_channels;
        length -= skip;
    }
    int trim = file->current_index * 1024 - (file->info.num_blocks * 1024 - file->info.num_ignored_samples_last);
    if (trim > 0)
        length -= trim;
    *num_samples = length;
    memcpy(buffer, wave_ptr, sizeof(double) * length * file->info.num_channels);
    return kHcaSuccess;
}

void hca_file_free(HcaFile *file) {
    hca_file_info_free(&file->info);
    hca_decoder_free(&file->decoder);
}
