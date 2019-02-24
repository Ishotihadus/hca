#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <hca/file.h>
#include <hca/error.h>
#include <sndfile.h>
#include <math.h>
#include "writer.h"

#define VERSION "HCA Decoder 1.21.0"

void print_help(FILE *stream, const char **argv) {
    fprintf(stream,
        VERSION "\n"
        "\n"
        "usage: %s [options] input.hca\n"
        "\n"
        "options:\n"
        "  -o, --output      output filename\n"
        "  -t, --type        output file type (wav / raw / flac / ogg)\n"
        "  -f, --bits        output file format (e.g. u8 / s16 / f32)\n"
        "  -q, --quality     output file quality for flac / ogg\n"
        "                    from 0.0 (lower bitrate) to 1.0 (higher bitrate)\n"
        "\n"
        "  -a                set key (lower bits)\n"
        "  -b                set key (higher bits)\n"
        "  -k, --key         set key (in decimal)\n"
        "  -s, --sub-key     set sub key (in decimal)\n"
        "  -v, --volume      set volume\n"
        "      --ignore-rva  ignore relative volume info in hca file\n"
        "  -n, --normalize   normalize waveforms not to be clipped\n"
        "\n"
        "  -i, --info        show info about input file and quit\n"
        "      --verbose     verbose output\n"
        "  -h, --help        show this help message\n"
        "      --version     show version info\n"
    , argv[0]);
}

void print_error(const char *message, const char **argv) {
    fprintf(stderr, "%s\n\n", message);
    print_help(stderr, argv);
}

void print_info(FILE *stream, HcaFileInfo *info, const char *input) {
    if (input)
        fprintf(stream, "Input File     : '%s'\n", input);
    else
        fprintf(stream, "Input File     : (stdin)\n");
    hca_file_info_print(info, stream);
}

int show_info(const char *input) {
    FILE *ifp = NULL;
    if (strcmp(input, "-") == 0) {
        ifp = stdin;
    } else {
        ifp = fopen(input, "rb");
        if (ifp == NULL) {
            fprintf(stderr, "Cannot open file: %s\n", input);
            return 1;
        }
    }
    HcaFileInfo info;
    HcaError error;
    if ((error = hca_file_info_init(&info, ifp))) {
        fprintf(stderr, "Hca Info Read Error: %s\n", hca_get_error_message(error));
    } else {
        printf("\n");
        print_info(stdout, &info, ifp == stdin ? NULL : input);
        printf("\n");
        hca_file_info_free(&info);
    }
    if (ifp != stdin)
        fclose(ifp);
    return 0;
}

int main(int argc, char const *argv[]) {
    const char *input = NULL;
    char *output = NULL;
    bool show_info_flag = false;
    bool ignore_rva = false;
    bool verbose = false;
    bool ignore_options = false;
    uint64_t key = 765765765765765ULL;
    uint16_t sub_key = 0;
    WriterConfig config = { .format = 0, .normalize = false, .volume = 1, .quality = NAN };

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] && !ignore_options) {
            if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--info") == 0) {
                show_info_flag = true;
            } else if (strcmp(argv[i], "-a") == 0) {
                key = (key & 0xffffffff00000000) | (strtoul(argv[++i], NULL, 16) & 0xffffffff);
            } else if (strcmp(argv[i], "-b") == 0) {
                key = (key & 0xffffffff) | ((uint64_t)strtoul(argv[++i], NULL, 16) << 32);
            } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--key") == 0) {
                key = strtoull(argv[++i], NULL, 0);
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--sub-key") == 0) {
                sub_key = strtoul(argv[++i], NULL, 0);
            } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--volume") == 0) {
                config.volume = atof(argv[++i]);
            } else if (strcmp(argv[i], "--ignore-rva") == 0) {
                ignore_rva = true;
            } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--normalize") == 0 || strcmp(argv[i], "--normalise") == 0) {
                config.normalize = true;
            } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
                output = (char*)malloc(strlen(argv[++i]) + 1);
                if (output == NULL) {
                    fprintf(stderr, "Memory allocation failed");
                    return 1;
                }
                strcpy(output, argv[i]);
            } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--type") == 0) {
                const char *typestr = argv[++i];
                int format = 0;
                if (strcmp(typestr, "wav") == 0) {
                    format = SF_FORMAT_WAV;
                } else if (strcmp(typestr, "raw") == 0) {
                    format = SF_FORMAT_RAW;
                } else if (strcmp(typestr, "flac") == 0) {
                    format = SF_FORMAT_FLAC;
                } else if (strcmp(typestr, "ogg") == 0) {
                    format = SF_FORMAT_OGG;
                } else {
                    print_error("Unknown output format", argv);
                    return 1;
                }
                config.format = (config.format & ~SF_FORMAT_TYPEMASK) | format;
            } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--bits") == 0) {
                const char *bitstr = argv[++i];
                int format = 0;
                char bitchar = isdigit(bitstr[0]) ? '\0' : bitstr[0];
                char *endianstr;
                long bits = strtol(bitchar ? bitstr + 1 : bitstr, &endianstr, 10);
                switch (bits) {
                    case 8:
                        format = bitchar == '\0' || bitchar == 'u' ? SF_FORMAT_PCM_U8 : bitchar == 's' ? SF_FORMAT_PCM_S8 : 0;
                        break;
                    case 16:
                        format = bitchar == '\0' || bitchar == 's' ? SF_FORMAT_PCM_16 : 0;
                        break;
                    case 24:
                        format = bitchar == '\0' || bitchar == 's' ? SF_FORMAT_PCM_24 : 0;
                        break;
                    case 32:
                        format = bitchar == '\0' || bitchar == 's' ? SF_FORMAT_PCM_32 : bitchar == 'f' ? SF_FORMAT_FLOAT : 0;
                        break;
                    case 64:
                        format = bitchar == '\0' || bitchar == 'f' ? SF_FORMAT_DOUBLE : 0;
                        break;
                }
                if (!format) {
                    print_error("Unknown output bit type", argv);
                    return 1;
                }
                if (strcmp(endianstr, "le") == 0) {
                    format |= SF_ENDIAN_LITTLE;
                } else if (strcmp(endianstr, "be") == 0) {
                    format |= SF_ENDIAN_BIG;
                } else if (*endianstr != '\0') {
                    print_error("Unknown output bit type", argv);
                    return 1;
                }
                config.format = (config.format & ~(SF_FORMAT_TYPEMASK | SF_FORMAT_ENDMASK)) | format;
            } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quality") == 0) {
                config.quality = atof(argv[++i]);
            } else if (strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
            } else if (strcmp(argv[i], "--version") == 0) {
                printf(VERSION "\n");
                return 0;
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_help(stdout, argv);
                return 0;
            } else if (strcmp(argv[i], "--") == 0) {
                ignore_options = true;
            }
        } else {
            input = argv[i];
        }
    }

    if (!input) {
        print_error("Input file must be specified", argv);
        return 1;
    }

    if (verbose)
        fprintf(stderr, VERSION "\n");
    if (show_info_flag)
        return show_info(input);

    if (!output) {
        output = (char*)malloc(strlen(input) + 5);
        if (output == NULL) {
            fprintf(stderr, "Memory allocation failed.");
            return 1;
        }
        strcpy(output, input);
        char *pos = output + strlen(output);
        for (char *p = pos; p >= output && *p != '/'; p--) {
            if (*p == '.') {
                pos = p;
                break;
            }
        }
        switch (config.format & SF_FORMAT_TYPEMASK) {
            case 0:
                config.format = (config.format & ~SF_FORMAT_TYPEMASK) | SF_FORMAT_WAV;
            case SF_FORMAT_WAV:
                strcpy(pos, ".wav");
                break;
            case SF_FORMAT_RAW:
                strcpy(pos, ".raw");
                break;
            case SF_FORMAT_FLAC:
                strcpy(pos, ".flac");
                break;
            case SF_FORMAT_OGG:
                strcpy(pos, ".ogg");
                break;
        }
    } else if ((config.format & SF_FORMAT_TYPEMASK) == 0) {
        int format = 0;
        char *suffix = output + strlen(output);
        int suffix_len = 0;
        for (char *p = suffix; p >= output && *p != '/'; p--) {
            if (*p == '.') {
                suffix_len = suffix - p;
                suffix = p;
                break;
            }
        }
        if (strcasecmp(suffix, ".wav") == 0) {
            format = SF_FORMAT_WAV;
        } else if (strcasecmp(suffix, ".raw") == 0) {
            format = SF_FORMAT_RAW;
        } else if (strcasecmp(suffix, ".flac") == 0) {
            format = SF_FORMAT_FLAC;
        } else if (strcasecmp(suffix, ".ogg") == 0) {
            format = SF_FORMAT_OGG;
        } else {
            fprintf(stderr, "Cannot estimate format from the output file name.\n");
            return 1;
        }
        config.format = (config.format & ~SF_FORMAT_TYPEMASK) | format;
    }
    if ((config.format & SF_FORMAT_TYPEMASK) == SF_FORMAT_OGG) {
        config.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
    } else if ((config.format & SF_FORMAT_SUBMASK) == 0) {
        int format = 0;
        switch (config.format & SF_FORMAT_TYPEMASK) {
            case SF_FORMAT_WAV:
            case SF_FORMAT_FLAC:
                format = SF_FORMAT_PCM_16;
                break;
            case SF_FORMAT_OGG:
                format = SF_FORMAT_VORBIS;
                break;
            default:
                format = SF_FORMAT_FLOAT;
        }
        config.format = (config.format & ~SF_FORMAT_SUBMASK) | format;
    }
    if (isnan(config.quality)) {
        switch (config.format & SF_FORMAT_TYPEMASK) {
            case SF_FORMAT_FLAC:
                config.quality = 0;
                break;
            default:
                config.quality = 1;
        }
    }

    FILE *ifp = NULL;
    if (strcmp(input, "-") == 0) {
        ifp = stdin;
    } else {
        ifp = fopen(input, "rb");
        if (ifp == NULL) {
            fprintf(stderr, "Cannot open file: %s\n", input);
            return 1;
        }
    }

    HcaFile hca;
    HcaError error;
    if (sub_key)
        key *= (sub_key << 16) | ((unsigned short)~sub_key + 2);
    if ((error = hca_file_init(&hca, ifp, key))) {
        fprintf(stderr, "Hca Initialization Error: %s\n", hca_get_error_message(error));
        if (ifp != stdin)
            fclose(ifp);
        return 1;
    }

    if (verbose)
        print_info(stderr, &hca.info, ifp == stdin ? NULL : input);

    config.path = output;
    config.num_channels = hca.info.num_channels;
    config.sampling_rate = hca.info.sampling_rate;
    config.expected_num_samples = hca_file_info_get_num_samples(&hca.info);

    double *buffer = (double*)malloc(1024 * hca.info.num_channels * sizeof(double));
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed.");
        if (ifp != stdin)
            fclose(ifp);
        hca_file_free(&hca);
        return 1;
    }

    Writer writer;
    if (!writer_init(&writer, &config)) {
        if (ifp != stdin)
            fclose(ifp);
        hca_file_free(&hca);
        free(buffer);
        return 1;
    }

    size_t s;
    while ((error = hca_file_read(buffer, &s, false, &hca)) == kHcaSuccess && writer_write(&writer, buffer, s)) ;
    if (error && error != kHcaEndOfFile)
        fprintf(stderr, "Hca Decode Error: %s\n", hca_get_error_message(error));

    writer_finalize(&writer);
    writer_free(&writer);
    hca_file_free(&hca);
    free(buffer);
    if (ifp != stdin)
        fclose(ifp);

    return 0;
}
