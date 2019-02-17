#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include "writer.h"

bool writer_init(Writer *writer, WriterConfig *config) {
    writer->config = *config;
    SF_INFO sf_info = {
        .frames = config->expected_num_samples,
        .samplerate = config->sampling_rate,
        .channels = config->num_channels,
        .format = config->format,
        .sections = 0, .seekable = 0
    };
    if (sf_format_check(&sf_info) == SF_FALSE) {
        fprintf(stderr, "Invalid format\n");
        return false;
    }
    writer->sndfile = sf_open(config->path, SFM_WRITE, &sf_info);
    if (writer->sndfile == NULL) {
        fprintf(stderr, "Cannot open sndfile\n");
        return false;
    }
    sf_command(writer->sndfile, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE);
    sf_command(writer->sndfile, SFC_SET_CLIPPING, NULL, SF_TRUE);
    sf_command(writer->sndfile, SFC_SET_VBR_ENCODING_QUALITY, &config->quality, sizeof(double));
    writer->sync_write = !config->normalize;
    writer->buffer_capacity = 1024;
    if (config->expected_num_samples > 0 && !writer->sync_write)
        writer->buffer_capacity = config->expected_num_samples;
    writer->buffer = (double*)malloc(sizeof(double) * writer->buffer_capacity * config->num_channels);
    if (writer->buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        sf_close(writer->sndfile);
        writer->sndfile = NULL;
        return false;
    }
    writer->buffer_length = 0;
    writer->cursor = writer->buffer;
    return true;
}

bool writer_write(Writer *writer, const double *data, const size_t num_samples) {
    if (num_samples == 0)
        return true;
    size_t required_capacity = num_samples + writer->buffer_length;
    if (writer->buffer_capacity < required_capacity) {
        size_t capacity = writer->buffer_capacity * 2;
        while (capacity < required_capacity)
            capacity *= 2;
        double *ptr = (double*)realloc(writer->buffer, sizeof(double) * capacity * writer->config.num_channels);
        if (ptr == NULL) {
            fprintf(stderr, "Failed to reallocate memory\n");
            return false;
        }
        writer->buffer = ptr;
        writer->buffer_capacity = capacity;
    }
    size_t count = writer->config.num_channels * num_samples;
    if (writer->sync_write) {
        if (writer->config.volume == 1) {
            if ((size_t)sf_writef_double(writer->sndfile, data, num_samples) != num_samples) {
                fprintf(stderr, "Failed to write samples: %s\n", sf_strerror(writer->sndfile));
                return false;
            }
            return true;
        } else {
            for (size_t i = 0; i < count; i++)
                writer->cursor[i] = data[i] * writer->config.volume;
            if ((size_t)sf_writef_double(writer->sndfile, writer->cursor, num_samples) != num_samples) {
                fprintf(stderr, "Failed to write samples: %s\n", sf_strerror(writer->sndfile));
                return false;
            }
            return true;
        }
    } else {
        if (writer->config.volume == 1)
            memcpy(writer->cursor, data, sizeof(double) * count);
        else
            for (size_t i = 0; i < count; i++)
                writer->cursor[i] = data[i] * writer->config.volume;
        writer->cursor += count;
        writer->buffer_length += num_samples;
    }
    return true;
}

void writer_normalize(Writer *writer) {
    double min_limit = -1, max_limit = 1;
    switch (writer->config.format & SF_FORMAT_SUBMASK) {
        case SF_FORMAT_PCM_S8:
        case SF_FORMAT_PCM_U8:
            max_limit = 127.0 / 128.0;
            break;
        case SF_FORMAT_PCM_16:
            max_limit = 32767.0 / 32768.0;
            break;
        case SF_FORMAT_PCM_24:
            max_limit = 8388607.0 / 8388608.0;
            break;
        case SF_FORMAT_PCM_32:
            max_limit = 2147483647.0 / 2147483648.0;
            break;
    }
    double scale = 1;
    for (double *p = writer->buffer; p < writer->cursor; p++) {
        double this_scale = 1;
        if (*p < min_limit)
            this_scale = *p / min_limit;
        else if (*p > max_limit)
            this_scale = *p / max_limit;
        if (this_scale > scale)
            scale = this_scale;
    }
    if (scale != 1)
        for (double *p = writer->buffer; p < writer->cursor; p++)
            *p /= scale;
}

bool writer_finalize(Writer *writer) {
    if (writer->config.normalize)
        writer_normalize(writer);
    if ((writer->config.format & SF_FORMAT_SUBMASK) == SF_FORMAT_VORBIS) {
        const size_t block_size = 65536;
        double *cur = writer->buffer;
        for (size_t remain = writer->buffer_length; remain > 0;) {
            size_t s = remain > block_size ? block_size : remain;
            if ((size_t)sf_writef_double(writer->sndfile, cur, s) != s) {
                fprintf(stderr, "Failed to write samples: %s\n", sf_strerror(writer->sndfile));
                return false;
            }
            remain -= s;
            cur += s * writer->config.num_channels;
        }
        return true;
    } else {
        if ((size_t)sf_writef_double(writer->sndfile, writer->buffer, writer->buffer_length) != writer->buffer_length) {
            fprintf(stderr, "Failed to write samples: %s\n", sf_strerror(writer->sndfile));
            return false;
        }
        return true;
    }
}

bool writer_free(Writer *writer) {
    free(writer->buffer);
    sf_close(writer->sndfile);
    return true;
}
