#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct HcaDecryptor {
    int type;
    uint64_t key;
    uint8_t table[256];
    uint16_t table_large[65536];
} HcaDecryptor;

enum HcaError hca_decryptor_init_table(HcaDecryptor *decryptor);
enum HcaError hca_decryptor_decrypt(const HcaDecryptor *decryptor, uint8_t *data, const size_t len);
