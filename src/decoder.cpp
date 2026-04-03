/**
 * nImage - Native Image Decoder Implementation
 */

#include "decoder.h"
#include <cstring>
#include <algorithm>

// ============================================================================
// Format Detection
// ============================================================================

FormatDetection ImageDecoder::detectFormat(const uint8_t* buffer, size_t size) {
    FormatDetection result;

    if (buffer == nullptr || size < 12) {
        return result;
    }

    // Check RAW formats (based on file signatures)
    if (ImageFormatUtil::isRAW(buffer, size)) {
        // Further distinguish RAW formats
        // For now, treat all unknown RAWs as DNG
        result.format = ImageFormat::DNG;
        result.confidence = 0.7f;
        result.mimeType = "image/x-adobe-dng";
        return result;
    }

    // Check HEIC/HEIF
    if (ImageFormatUtil::isHEIC(buffer, size)) {
        result.format = ImageFormat::HEIC;
        result.confidence = 0.95f;
        result.mimeType = "image/heic";
        return result;
    }

    // Check standard formats
    if (ImageFormatUtil::isJPEG(buffer, size)) {
        result.format = ImageFormat::JPEG;
        result.confidence = 0.95f;
        result.mimeType = "image/jpeg";
        return result;
    }

    if (ImageFormatUtil::isPNG(buffer, size)) {
        result.format = ImageFormat::PNG;
        result.confidence = 0.95f;
        result.mimeType = "image/png";
        return result;
    }

    if (ImageFormatUtil::isTIFF(buffer, size)) {
        result.format = ImageFormat::TIFF;
        result.confidence = 0.95f;
        result.mimeType = "image/tiff";
        return result;
    }

    if (ImageFormatUtil::isWebP(buffer, size)) {
        result.format = ImageFormat::WEBP;
        result.confidence = 0.95f;
        result.mimeType = "image/webp";
        return result;
    }

    return result;
}

// ============================================================================
// Factory Methods
// ============================================================================

ImageDecoder* ImageDecoder::createDecoder(ImageFormat format) {
    switch (format) {
        case ImageFormat::CR2:
        case ImageFormat::NEF:
        case ImageFormat::ARW:
        case ImageFormat::ORF:
        case ImageFormat::RAF:
        case ImageFormat::RW2:
        case ImageFormat::DNG:
        case ImageFormat::PEFR:
        case ImageFormat::SRW:
        case ImageFormat::RWL:
            return new LibRawDecoder();

        case ImageFormat::HEIC:
        case ImageFormat::HEIF:
        case ImageFormat::AVIF:
            return new LibHeifDecoder();

        default:
            return nullptr;
    }
}

ImageDecoder* ImageDecoder::createDecoderForBuffer(const uint8_t* buffer, size_t size) {
    FormatDetection detected = detectFormat(buffer, size);
    if (detected.format == ImageFormat::UNKNOWN) {
        return nullptr;
    }
    return createDecoder(detected.format);
}

// ============================================================================
// LibRaw Decoder
// ============================================================================

LibRawDecoder::LibRawDecoder() : libraw_data_(nullptr) {
    // Note: In full implementation, this would initialize libraw
    // For now, we set up the opaque handle
}

LibRawDecoder::~LibRawDecoder() {
    close();
}

bool LibRawDecoder::openBuffer(const uint8_t* buffer, size_t size) {
    // Note: Full implementation would use libraw's open_buffer()
    // For now, just verify we have valid data
    if (!buffer || size < 64) {
        error_ = "Buffer too small or null";
        return false;
    }
    return true;
}

void LibRawDecoder::close() {
    // Note: Full implementation would call libraw_close()
    libraw_data_ = nullptr;
}

bool LibRawDecoder::decode(const uint8_t* buffer, size_t size, ImageData& output) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    // TODO: Full libraw implementation
    // Steps:
    // 1. libraw_open_buffer() or libraw_open()
    // 2. libraw_unpack()
    // 3. libraw_dcraw_process() or libraw_dcraw_ppm_tiff_writer()
    // 4. Extract pixel data to ImageData structure

    error_ = "LibRaw decoder not yet implemented - dependencies not bundled";
    return false;
}

bool LibRawDecoder::getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    // TODO: Full libraw implementation
    // Use libraw_get_i_thumbnail(), libraw_get_exif_data(), etc.

    error_ = "LibRaw decoder not yet implemented - dependencies not bundled";
    return false;
}

bool LibRawDecoder::supportsFormat(ImageFormat format) const {
    switch (format) {
        case ImageFormat::CR2:
        case ImageFormat::NEF:
        case ImageFormat::ARW:
        case ImageFormat::ORF:
        case ImageFormat::RAF:
        case ImageFormat::RW2:
        case ImageFormat::DNG:
        case ImageFormat::PEFR:
        case ImageFormat::SRW:
        case ImageFormat::RWL:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// LibHeif Decoder
// ============================================================================

LibHeifDecoder::LibHeifDecoder() : heif_context_(nullptr) {
    // Note: In full implementation, this would initialize libheif
}

LibHeifDecoder::~LibHeifDecoder() {
    close();
}

bool LibHeifDecoder::openBuffer(const uint8_t* buffer, size_t size) {
    // Note: Full implementation would use heif_context_alloc() and heif_read_from_memory()
    if (!buffer || size < 12) {
        error_ = "Buffer too small or null";
        return false;
    }
    return true;
}

void LibHeifDecoder::close() {
    // Note: Full implementation would call heif_context_free()
    heif_context_ = nullptr;
}

bool LibHeifDecoder::decodeHeif(ImageData& output) {
    // TODO: Full libheif implementation
    // Steps:
    // 1. heif_context_alloc()
    // 2. heif_context_read_from_memory()
    // 3. heif_context_get_primary_image_handle()
    // 4. heif_image_create()
    // 5. heif_image_add_plane() for each channel
    // 6. heif_image_decode()
    // 7. Extract to ImageData structure
    return false;
}

bool LibHeifDecoder::extractMetadata(ImageMetadata& metadata) {
    // TODO: Implement metadata extraction from libheif
    return false;
}

bool LibHeifDecoder::decode(const uint8_t* buffer, size_t size, ImageData& output) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    // TODO: Full libheif implementation

    error_ = "LibHeif decoder not yet implemented - dependencies not bundled";
    return false;
}

bool LibHeifDecoder::getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    // TODO: Full libheif implementation

    error_ = "LibHeif decoder not yet implemented - dependencies not bundled";
    return false;
}

bool LibHeifDecoder::supportsFormat(ImageFormat format) const {
    switch (format) {
        case ImageFormat::HEIC:
        case ImageFormat::HEIF:
        case ImageFormat::AVIF:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Format Utilities
// ============================================================================

namespace ImageFormatUtil {

ImageFormat formatFromString(const std::string& format) {
    if (format == "cr2") return ImageFormat::CR2;
    if (format == "nef") return ImageFormat::NEF;
    if (format == "arw") return ImageFormat::ARW;
    if (format == "orf") return ImageFormat::ORF;
    if (format == "raf") return ImageFormat::RAF;
    if (format == "rw2") return ImageFormat::RW2;
    if (format == "dng") return ImageFormat::DNG;
    if (format == "heic") return ImageFormat::HEIC;
    if (format == "heif") return ImageFormat::HEIF;
    if (format == "avif") return ImageFormat::AVIF;
    if (format == "jpeg" || format == "jpg") return ImageFormat::JPEG;
    if (format == "png") return ImageFormat::PNG;
    if (format == "tiff" || format == "tif") return ImageFormat::TIFF;
    if (format == "webp") return ImageFormat::WEBP;
    if (format == "gif") return ImageFormat::GIF;
    return ImageFormat::UNKNOWN;
}

std::string formatToString(ImageFormat format) {
    switch (format) {
        case ImageFormat::CR2: return "cr2";
        case ImageFormat::NEF: return "nef";
        case ImageFormat::ARW: return "arw";
        case ImageFormat::ORF: return "orf";
        case ImageFormat::RAF: return "raf";
        case ImageFormat::RW2: return "rw2";
        case ImageFormat::DNG: return "dng";
        case ImageFormat::PEFR: return "pef";
        case ImageFormat::SRW: return "srw";
        case ImageFormat::RWL: return "rwl";
        case ImageFormat::HEIC: return "heic";
        case ImageFormat::HEIF: return "heif";
        case ImageFormat::AVIF: return "avif";
        case ImageFormat::JPEG: return "jpeg";
        case ImageFormat::PNG: return "png";
        case ImageFormat::TIFF: return "tiff";
        case ImageFormat::WEBP: return "webp";
        case ImageFormat::GIF: return "gif";
        default: return "unknown";
    }
}

std::string formatToMimeType(ImageFormat format) {
    switch (format) {
        case ImageFormat::CR2: return "image/x-canon-cr2";
        case ImageFormat::NEF: return "image/x-nikon-nef";
        case ImageFormat::ARW: return "image/x-sony-arw";
        case ImageFormat::ORF: return "image/x-olympus-orf";
        case ImageFormat::RAF: return "image/x-fuji-raf";
        case ImageFormat::RW2: return "image/x-panasonic-rw2";
        case ImageFormat::DNG: return "image/x-adobe-dng";
        case ImageFormat::PEFR: return "image/x-pentax-pef";
        case ImageFormat::SRW: return "image/x-samsung-srw";
        case ImageFormat::RWL: return "image/x-leica-rwl";
        case ImageFormat::HEIC: return "image/heic";
        case ImageFormat::HEIF: return "image/heif";
        case ImageFormat::AVIF: return "image/avif";
        case ImageFormat::JPEG: return "image/jpeg";
        case ImageFormat::PNG: return "image/png";
        case ImageFormat::TIFF: return "image/tiff";
        case ImageFormat::WEBP: return "image/webp";
        case ImageFormat::GIF: return "image/gif";
        default: return "application/octet-stream";
    }
}

const char* formatToExtension(ImageFormat format) {
    switch (format) {
        case ImageFormat::CR2: return "cr2";
        case ImageFormat::NEF: return "nef";
        case ImageFormat::ARW: return "arw";
        case ImageFormat::ORF: return "orf";
        case ImageFormat::RAF: return "raf";
        case ImageFormat::RW2: return "rw2";
        case ImageFormat::DNG: return "dng";
        case ImageFormat::PEFR: return "pef";
        case ImageFormat::SRW: return "srw";
        case ImageFormat::RWL: return "rwl";
        case ImageFormat::HEIC: return "heic";
        case ImageFormat::HEIF: return "heif";
        case ImageFormat::AVIF: return "avif";
        case ImageFormat::JPEG: return "jpg";
        case ImageFormat::PNG: return "png";
        case ImageFormat::TIFF: return "tiff";
        case ImageFormat::WEBP: return "webp";
        case ImageFormat::GIF: return "gif";
        default: return "bin";
    }
}

// Magic byte detection
bool isJPEG(const uint8_t* buffer, size_t size) {
    if (size < 3) return false;
    return buffer[0] == 0xFF && buffer[1] == 0xD8 && buffer[2] == 0xFF;
}

bool isPNG(const uint8_t* buffer, size_t size) {
    if (size < 8) return false;
    return buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47;
}

bool isTIFF(const uint8_t* buffer, size_t size) {
    if (size < 4) return false;
    // Big-endian: MM (0x4D4D), Little-endian: II (0x4949)
    return (buffer[0] == 0x4D && buffer[1] == 0x4D) ||
           (buffer[0] == 0x49 && buffer[1] == 0x49);
}

bool isWebP(const uint8_t* buffer, size_t size) {
    if (size < 12) return false;
    // RIFF....WEBP
    return buffer[0] == 0x52 && buffer[1] == 0x49 && buffer[2] == 0x46 && buffer[3] == 0x46 &&
           buffer[8] == 0x57 && buffer[9] == 0x45 && buffer[10] == 0x42 && buffer[11] == 0x50;
}

bool isHEIC(const uint8_t* buffer, size_t size) {
    if (size < 12) return false;
    // HEIC files start with ftyp box, brand should be heic, heif, mif1, etc.
    if (buffer[4] != 0x66 || buffer[5] != 0x74 || buffer[6] != 0x79 || buffer[7] != 0x70) {
        return false;
    }
    // Check for heic, heif, mif1, or avif brands at offset 8
    const char* brand = reinterpret_cast<const char*>(buffer + 8);
    return (strncmp(brand, "heic", 4) == 0) ||
           (strncmp(brand, "heif", 4) == 0) ||
           (strncmp(brand, "mif1", 4) == 0) ||
           (strncmp(brand, "avif", 4) == 0) ||
           (strncmp(brand, "avis", 4) == 0);
}

bool isRAW(const uint8_t* buffer, size_t size) {
    if (size < 16) return false;

    // Check for various RAW format signatures
    // Canon CR2/CRW: starts with "II" or "MM" followed by TIFF data
    // Nikon NEF: same TIFF-based structure
    // Sony ARW: same
    // Olympus ORF: same
    // Fujifilm RAF: "FUJIFILM" at offset 0
    // Panasonic RW2: "II" followed by "PAnasonic" or similar

    // TIFF-based RAW formats
    if (isTIFF(buffer, size)) {
        // Could be CR2, NEF, ARW, ORF, RW2, DNG, etc.
        // Additional heuristics would distinguish them
        return true;
    }

    // Fujifilm RAF signature
    if (size >= 8 && strncmp(reinterpret_cast<const char*>(buffer), "FUJIFILM", 8) == 0) {
        return true;
    }

    // Pentax PEF signature
    if (size >= 6 && strncmp(reinterpret_cast<const char*>(buffer), "PENTAX", 6) == 0) {
        return true;
    }

    // Samsung SRW signature
    if (size >= 4 && strncmp(reinterpret_cast<const char*>(buffer), "SRW", 3) == 0) {
        return true;
    }

    return false;
}

} // namespace ImageFormatUtil
