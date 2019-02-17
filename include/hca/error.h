#pragma once

typedef enum HcaError {
    kHcaSuccess = 0,
    kHcaMemoryAllocationFailed,
    kHcaInvalidCheckSum,
    kHcaInvalidFileSignature,
    kHcaInvalidBlockSignature,
    kHcaFileSizeNotSufficient,
    kHcaBlockSizeNotSufficient,
    kHcaHeaderNotSufficient,
    kHcaUnknownChunk,
    kHcaInvalidBlockSize,
    kHcaVBRNotSupported,
    kHcaInvalidCompressionParameter,
    kHcaUnsupportedCompressionParameter,
    kHcaInvalidAthType,
    kHcaInvalidEnryptionType,
    kHcaInternalError,
    kHcaEndOfFile = -1
} HcaError;

const char *hca_get_error_message(HcaError);
