#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sndfile.h>

typedef struct {
    const char *path;
    int format;
    unsigned int num_channels;
    unsigned int sampling_rate;
    bool normalize;
    double volume;
    size_t expected_num_samples;
    double quality;
} WriterConfig;

typedef struct {
    WriterConfig config;
    SNDFILE *sndfile;
    double *buffer;
    double *cursor;
    size_t buffer_capacity;
    size_t buffer_length;
    bool sync_write;
} Writer;

bool writer_init(Writer *writer, WriterConfig *config);
bool writer_write(Writer *writer, const double *data, const size_t num_samples);
bool writer_finalize(Writer *writer);
bool writer_free(Writer *writer);
