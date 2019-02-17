#include <hca/error.h>

const char *hca_get_error_message(HcaError error) {
    switch (error) {
        case kHcaSuccess:
            return "success";
        case kHcaMemoryAllocationFailed:
            return "memory allocation failed";
        case kHcaInvalidCheckSum:
            return "incorrect checksum";
        case kHcaInvalidFileSignature:
            return "invalid file signature";
        case kHcaInvalidBlockSignature:
            return "invalid block signature";
        case kHcaFileSizeNotSufficient:
            return "too short file";
        case kHcaBlockSizeNotSufficient:
            return "too short block size";
        case kHcaHeaderNotSufficient:
            return "missing necessary header chunk(s)";
        case kHcaUnknownChunk:
            return "unknown name of chunk(s)";
        case kHcaInvalidBlockSize:
            return "invalid block size";
        case kHcaVBRNotSupported:
            return "VBR is not supported";
        case kHcaInvalidCompressionParameter:
            return "invalid parameter(s) for decoding";
        case kHcaUnsupportedCompressionParameter:
            return "unsupported parameter(s) for decoding";
        case kHcaInvalidAthType:
            return "unsupported ATH type";
        case kHcaInvalidEnryptionType:
            return "unsupported encryption type";
        case kHcaInternalError:
            return "internal error";
        case kHcaEndOfFile:
            return "EOF";
    }
    return "unknown error";
}
