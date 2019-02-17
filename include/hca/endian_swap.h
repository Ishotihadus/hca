#pragma once
#include <stdint.h>

#if HAVE_BYTESWAP_H
#include <byteswap.h>
#elif defined __APPLE__
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#else
#define bswap_16(x) ((uint16_t)((((uint16_t)(x) & 0xff00) >> 8) | (((uint16_t)(x) & 0x00ff) << 8)))
#define bswap_32(x) ((uint32_t)((((uint32_t)(x) & 0xff000000U) >> 24) | (((uint32_t)(x) & 0x00ff0000U) >> 8) | (((uint32_t)(x) & 0x0000ff00U) << 8) | (((uint32_t)(x) & 0x000000ffU) << 24)))
#define bswap_64(x) ((uint64_t)(((uint64_t)bswap_32(((uint64_t)(x) & 0xffffffff00000000ULL) >> 32)) | (((uint64_t)bswap_32((uint64_t)(x) & 0x00000000ffffffffULL)) << 32)))
#endif

#if defined __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define bswapl_16(x) (x)
#define bswapl_32(x) (x)
#define bswapl_64(x) (x)
#define bswapb_16(x) (bswap_16(x))
#define bswapb_32(x) (bswap_32(x))
#define bswapb_64(x) (bswap_64(x))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define bswapl_16(x) (bswap_16(x))
#define bswapl_32(x) (bswap_32(x))
#define bswapl_64(x) (bswap_64(x))
#define bswapb_16(x) (x)
#define bswapb_32(x) (x)
#define bswapb_64(x) (x)
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#define bswapl_16(x) (x)
#define bswapl_32(x) ((((x) & 0xffff) << 16) | ((x) >> 16))
#define bswapl_64(x) (((uint64_t)bswapl_32((uint32_t)((x) & 0xffffffff)) << 32) | (uint64_t)bswapl_32((uint32_t)((x) >> 32)))
#define bswapb_16(x) (bswap_16(x))
#define bswapb_32(x) ((((uint32_t)bswap_16((uint16_t)((x) >> 16))) << 16) | ((uint32_t)bswap_16((uint16_t)((x) & 0xffff))))
#define bswapb_64(x) ((((uint64_t)bswapb_32((uint32_t)((x) >> 32))) << 32) | ((uint64_t)bswapb_32((uint32_t)((x) & 0xffffffff))))
#endif
#else
static inline char is_little_endian() {
    int i = 1;
    return *(char*)&i;
}
#define bswapl_16(x) (is_little_endian() ? x : bswap_16(x))
#define bswapl_32(x) (is_little_endian() ? x : bswap_32(x))
#define bswapl_64(x) (is_little_endian() ? x : bswap_64(x))
#define bswapb_16(x) (is_little_endian() ? bswap_16(x) : x)
#define bswapb_32(x) (is_little_endian() ? bswap_32(x) : x)
#define bswapb_64(x) (is_little_endian() ? bswap_64(x) : x)
#endif
