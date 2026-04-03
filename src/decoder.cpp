/**
 * nImage - Native Image Decoder Implementation
 */

#include "decoder.h"
#include <libraw/libraw.h>
#include <cstring>
#include <algorithm>
#include <cmath>

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
    // Initialize libraw
    libraw_data_ = new LibRaw();
}

LibRawDecoder::~LibRawDecoder() {
    close();
    if (libraw_data_) {
        delete static_cast<LibRaw*>(libraw_data_);
        libraw_data_ = nullptr;
    }
}

bool LibRawDecoder::openBuffer(const uint8_t* buffer, size_t size) {
    if (!libraw_data_ || !buffer || size < 64) {
        error_ = "Invalid buffer or size too small";
        return false;
    }

    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Open the buffer
    int ret = raw->open_buffer((void*)buffer, size);
    if (ret != 0) {
        error_ = "Failed to open RAW buffer (error code: " + std::to_string(ret) + ")";
        return false;
    }

    return true;
}

void LibRawDecoder::close() {
    if (libraw_data_) {
        LibRaw* raw = static_cast<LibRaw*>(libraw_data_);
        raw->recycle();
        // Note: LibRaw 0.22 has no close() method - recycle() handles cleanup
    }
}

bool LibRawDecoder::decode(const uint8_t* buffer, size_t size, ImageData& output) {
    if (!buffer || size < 64) {
        error_ = "Buffer too small or null";
        return false;
    }

    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Open buffer (LibRaw maintains state, so we always open for each decode)
    int ret = raw->open_buffer((void*)buffer, size);
    if (ret != 0) {
        error_ = "Failed to open RAW buffer (error code: " + std::to_string(ret) + ")";
        return false;
    }

    // Unpack the RAW data
    ret = raw->unpack();
    if (ret != 0) {
        error_ = "Failed to unpack RAW data (error code: " + std::to_string(ret) + ")";
        return false;
    }

    // Process with dcraw - this demosaics and converts to image
    ret = raw->dcraw_process();
    if (ret != 0) {
        error_ = "Failed to process RAW data (error code: " + std::to_string(ret) + ")";
        return false;
    }

    // Extract to output
    return processRAW(output);
}

bool LibRawDecoder::processRAW(ImageData& output) {
    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Get the processed image data
    // After dcraw_process(), data is in imgdata.image as 16-bit RGB
    libraw_processed_image_t* img = raw->dcraw_make_mem_image(nullptr);
    if (!img) {
        error_ = "Failed to extract processed image from libraw";
        return false;
    }

    // Use dimensions from the returned image structure
    int width = img->width;
    int height = img->height;
    int colors = img->colors;
    int bits = img->bits;

    if (width <= 0 || height <= 0) {
        error_ = "Invalid output dimensions from RAW";
        return false;
    }

    // Copy dimensions
    output.width = width;
    output.height = height;
    output.bitsPerChannel = bits;  // Use actual bits from img
    output.channels = colors;
    output.hasAlpha = (colors == 4);
    output.colorSpace = "sRGB";  // Default, could be Adobe RGB based on camera

    // Set format from what libraw detected
    output.format = ImageFormatUtil::formatToString(
        ImageFormatUtil::formatFromString(raw->imgdata.idata.model)
    );
    if (output.format == "unknown") {
        output.format = "raw";
    }

    // Copy pixel data
    size_t dataSize = img->width * img->height * img->colors;
    output.data.resize(dataSize);
    std::memcpy(output.data.data(), img->data, dataSize);

    // Copy metadata
    extractMetadataToImageData(output);

    // Free the memory
    LibRaw::dcraw_clear_mem(img);

    return true;
}

void LibRawDecoder::extractMetadataToImageData(ImageData& output) {
    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Camera info
    if (raw->imgdata.idata.make[0]) {
        output.cameraMake = raw->imgdata.idata.make;
    }
    if (raw->imgdata.idata.model[0]) {
        output.cameraModel = raw->imgdata.idata.model;
    }

    // Capture settings
    if (raw->imgdata.other.shutter > 0) {
        output.exposureTime = raw->imgdata.other.shutter;
    }
    if (raw->imgdata.other.aperture > 0) {
        output.fNumber = raw->imgdata.other.aperture;
    }
    if (raw->imgdata.other.iso_speed > 0) {
        output.isoSpeed = raw->imgdata.other.iso_speed;
    }
    if (raw->imgdata.other.focal_len > 0) {
        output.focalLength = raw->imgdata.other.focal_len;
    }

    // Date/time (LibRaw 0.22 uses timestamp instead of datetime string)
    if (raw->imgdata.other.timestamp > 0) {
        char datetimeBuf[64];
        strftime(datetimeBuf, sizeof(datetimeBuf), "%Y:%m:%d %H:%M:%S", localtime(&raw->imgdata.other.timestamp));
        output.dateTime = datetimeBuf;
    }

    // RAW dimensions (sensor)
    output.rawWidth = raw->imgdata.sizes.raw_width;
    output.rawHeight = raw->imgdata.sizes.raw_height;

    // Orientation
    output.orientation = raw->imgdata.sizes.flip;
    if (output.orientation < 1 || output.orientation > 8) {
        output.orientation = 1;
    }
}

bool LibRawDecoder::getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) {
    if (!buffer || size < 64) {
        error_ = "Buffer too small or null";
        return false;
    }

    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Open and unpack only (no full processing)
    int ret = raw->open_buffer((void*)buffer, size);
    if (ret != 0) {
        error_ = "Failed to open RAW buffer (error code: " + std::to_string(ret) + ")";
        return false;
    }

    ret = raw->unpack();
    if (ret != 0) {
        error_ = "Failed to unpack RAW data (error code: " + std::to_string(ret) + ")";
        return false;
    }

    // Extract metadata without full decode
    extractMetadata(metadata);

    return true;
}

bool LibRawDecoder::extractMetadata(ImageMetadata& metadata) {
    LibRaw* raw = static_cast<LibRaw*>(libraw_data_);

    // Format info
    metadata.format = raw->imgdata.idata.model[0] ?
        ImageFormatUtil::formatToString(
            ImageFormatUtil::formatFromString(raw->imgdata.idata.model)) : "raw";

    // Dimensions (LibRaw 0.22 uses width/height, not output_width/output_height)
    metadata.width = raw->imgdata.sizes.width;
    metadata.height = raw->imgdata.sizes.height;
    metadata.rawWidth = raw->imgdata.sizes.raw_width;
    metadata.rawHeight = raw->imgdata.sizes.raw_height;

    // Camera info
    if (raw->imgdata.idata.make[0]) {
        metadata.cameraMake = raw->imgdata.idata.make;
    }
    if (raw->imgdata.idata.model[0]) {
        metadata.cameraModel = raw->imgdata.idata.model;
    }

    // Capture settings
    metadata.isoSpeed = raw->imgdata.other.iso_speed;
    metadata.exposureTime = raw->imgdata.other.shutter;
    metadata.fNumber = raw->imgdata.other.aperture;
    metadata.focalLength = raw->imgdata.other.focal_len;

    // Color
    metadata.colorSpace = "sRGB";
    metadata.hasAlpha = false;
    metadata.bitsPerSample = 16;  // RAW is typically 12-14 bit, we output as 16

    // Orientation
    metadata.orientation = raw->imgdata.sizes.flip;
    if (metadata.orientation < 1 || metadata.orientation > 8) {
        metadata.orientation = 1;
    }

    // Lens (LibRaw 0.22 uses Lens from makernotes, not lensmodel directly)
    if (raw->imgdata.lens.makernotes.Lens[0]) {
        metadata.lensModel = raw->imgdata.lens.makernotes.Lens;
    }

    return true;
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

LibHeifDecoder::LibHeifDecoder() : heif_context_(nullptr), heif_handle_(nullptr) {
}

LibHeifDecoder::~LibHeifDecoder() {
    close();
}

bool LibHeifDecoder::openBuffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 12) {
        error_ = "Buffer too small or null";
        return false;
    }

    // Free any existing context
    close();

    // Allocate new context
    heif_context_ = heif_context_alloc();
    if (!heif_context_) {
        error_ = "Failed to allocate HEIF context";
        return false;
    }

    // Read from memory
    heif_error err = heif_context_read_from_memory_without_copy(
        heif_context_, buffer, size, nullptr);
    if (err.code != heif_error_Ok) {
        error_ = std::string("Failed to read HEIF: ") + err.message;
        heif_context_free(heif_context_);
        heif_context_ = nullptr;
        return false;
    }

    return true;
}

void LibHeifDecoder::close() {
    if (heif_handle_) {
        heif_image_handle_release(heif_handle_);
        heif_handle_ = nullptr;
    }
    if (heif_context_) {
        heif_context_free(heif_context_);
        heif_context_ = nullptr;
    }
}

bool LibHeifDecoder::decodeHeif(ImageData& output) {
    if (!heif_context_) {
        error_ = "No HEIF context - call openBuffer first";
        return false;
    }

    // Get primary image handle
    heif_image_handle* handle = nullptr;
    heif_error err = heif_context_get_primary_image_handle(heif_context_, &handle);
    if (err.code != heif_error_Ok) {
        error_ = std::string("Failed to get primary image: ") + err.message;
        return false;
    }

    // Store handle for later release
    if (heif_handle_) {
        heif_image_handle_release(heif_handle_);
    }
    heif_handle_ = handle;

    // Get image dimensions
    int width = heif_image_handle_get_width(handle);
    int height = heif_image_handle_get_height(handle);
    if (width <= 0 || height <= 0) {
        error_ = "Invalid image dimensions from HEIF";
        return false;
    }

    // Check for alpha channel
    bool hasAlpha = heif_image_handle_has_alpha_channel(handle) != 0;

    // Decode to RGB or RGBA
    heif_image* img = nullptr;
    enum heif_colorspace colorspace = heif_colorspace_RGB;
    enum heif_chroma chroma = hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;

    err = heif_decode_image(handle, &img, colorspace, chroma, nullptr);
    if (err.code != heif_error_Ok) {
        error_ = std::string("Failed to decode HEIF: ") + err.message;
        return false;
    }

    // Get output dimensions
    int outWidth = heif_image_get_primary_width(img);
    int outHeight = heif_image_get_primary_height(img);

    // Get chroma format
    enum heif_chroma outChroma = heif_image_get_chroma_format(img);
    int numChannels = (outChroma == heif_chroma_interleaved_RGBA ||
                       outChroma == heif_chroma_interleaved_RRGGBBAA_LE ||
                       outChroma == heif_chroma_interleaved_RRGGBBAA_BE) ? 4 : 3;

    // Get pixel data
    int stride = 0;
    const uint8_t* pixelData = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

    if (!pixelData) {
        error_ = "Failed to get pixel data from HEIF image";
        heif_image_release(img);
        return false;
    }

    // Copy to output structure
    output.width = outWidth;
    output.height = outHeight;
    output.bitsPerChannel = 8;  // libheif decodes to 8-bit output
    output.channels = numChannels;
    output.hasAlpha = hasAlpha;
    output.colorSpace = "sRGB";
    output.format = "heic";

    // Calculate data size (accounting for stride)
    // rowSize is the actual bytes per row (stride may be larger due to alignment)
    size_t rowSize = outWidth * numChannels;  // 8 bits per channel = 1 byte
    size_t dataSize = rowSize * outHeight;
    output.data.resize(dataSize);

    // Copy row by row (stride may be larger than actual row size)
    for (int y = 0; y < outHeight; y++) {
        std::memcpy(output.data.data() + y * rowSize, pixelData + y * stride, rowSize);
    }

    // Release the decoded image
    heif_image_release(img);

    // Extract metadata
    extractMetadataToImageData(output);

    return true;
}

void LibHeifDecoder::extractMetadataToImageData(ImageData& output) {
    if (!heif_handle_) {
        return;
    }

    // Basic metadata is already populated
    // HEIF files typically don't have the same camera metadata as RAW files
    output.hasAlpha = heif_image_handle_has_alpha_channel(heif_handle_) != 0;
}

bool LibHeifDecoder::extractMetadata(ImageMetadata& metadata) {
    if (!heif_context_) {
        error_ = "No HEIF context - call openBuffer first";
        return false;
    }

    heif_image_handle* handle = nullptr;
    heif_error err = heif_context_get_primary_image_handle(heif_context_, &handle);
    if (err.code != heif_error_Ok) {
        error_ = std::string("Failed to get primary image: ") + err.message;
        return false;
    }

    // Release previous handle if different
    if (heif_handle_ && heif_handle_ != handle) {
        heif_image_handle_release(heif_handle_);
    }
    heif_handle_ = handle;

    // Get dimensions
    metadata.width = heif_image_handle_get_width(handle);
    metadata.height = heif_image_handle_get_height(handle);

    // Get color info
    metadata.hasAlpha = heif_image_handle_has_alpha_channel(handle) != 0;
    metadata.bitsPerSample = heif_image_handle_get_luma_bits_per_pixel(handle);
    if (metadata.bitsPerSample < 0) {
        metadata.bitsPerSample = 8;
    }

    metadata.format = "heic";
    metadata.colorSpace = "sRGB";

    // Orientation - HEIF stores this, but we'd need to check the transformation properties
    // For now, default to 1
    metadata.orientation = 1;

    return true;
}

bool LibHeifDecoder::decode(const uint8_t* buffer, size_t size, ImageData& output) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    return decodeHeif(output);
}

bool LibHeifDecoder::getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) {
    if (!openBuffer(buffer, size)) {
        return false;
    }

    return extractMetadata(metadata);
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
