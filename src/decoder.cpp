/**
 * nImage - Native Image Decoder Implementation
 */

#include "decoder.h"
#include <libraw/libraw.h>

// ImageMagick configuration - must be defined before including MagickCore
#define MAGICKCORE_QUANTUM_DEPTH 16
#define MAGICKCORE_HDRI_ENABLE 0

#include <ImageMagick-7/MagickCore/MagickCore.h>
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

    // Check ImageMagick-handled formats
    if (ImageFormatUtil::isPSD(buffer, size)) {
        result.format = ImageFormat::PSD;
        result.confidence = 0.95f;
        result.mimeType = "image/vnd.adobe.photoshop";
        return result;
    }

    if (ImageFormatUtil::isPDF(buffer, size)) {
        result.format = ImageFormat::PDF;
        result.confidence = 0.95f;
        result.mimeType = "application/pdf";
        return result;
    }

    if (ImageFormatUtil::isEXR(buffer, size)) {
        result.format = ImageFormat::EXR;
        result.confidence = 0.95f;
        result.mimeType = "image/x-exr";
        return result;
    }

    if (ImageFormatUtil::isHDR(buffer, size)) {
        result.format = ImageFormat::HDR;
        result.confidence = 0.95f;
        result.mimeType = "image/vnd.radiance";
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
        case ImageFormat::CRW:
        case ImageFormat::MRW:
        case ImageFormat::NRW:
        case ImageFormat::PEF:
        case ImageFormat::ERF:
        case ImageFormat::GRB:
        case ImageFormat::F3FR:
        case ImageFormat::K25:
        case ImageFormat::KDC:
        case ImageFormat::MEF:
        case ImageFormat::MOS:
        case ImageFormat::MRAW:
        case ImageFormat::RRF:
        case ImageFormat::SR2:
        case ImageFormat::RWZ:
            return new LibRawDecoder();

        case ImageFormat::HEIC:
        case ImageFormat::HEIF:
        case ImageFormat::AVIF:
            return new LibHeifDecoder();

        default:
            return new MagickDecoder();
    }
}

ImageDecoder* ImageDecoder::createDecoderForBuffer(const uint8_t* buffer, size_t size) {
    FormatDetection detected = detectFormat(buffer, size);
    if (detected.format != ImageFormat::UNKNOWN) {
        return createDecoder(detected.format);
    }
    return new MagickDecoder();
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
// MagickDecoder (ImageMagick/MagickCore)
// ============================================================================

MagickDecoder::MagickDecoder() : imageInfo_(nullptr), exceptionInfo_(nullptr) {
    // Initialize ImageMagick
    MagickCoreGenesis(nullptr, MagickTrue);

    // Create ImageInfo and ExceptionInfo
    imageInfo_ = CloneImageInfo(nullptr);
    exceptionInfo_ = AcquireExceptionInfo();
}

MagickDecoder::~MagickDecoder() {
    if (imageInfo_) {
        DestroyImageInfo(static_cast<ImageInfo*>(imageInfo_));
        imageInfo_ = nullptr;
    }
    if (exceptionInfo_) {
        DestroyExceptionInfo(static_cast<ExceptionInfo*>(exceptionInfo_));
        exceptionInfo_ = nullptr;
    }
    MagickCoreTerminus();
}

bool MagickDecoder::openBuffer(const uint8_t* buffer, size_t size) {
    // Buffer reading is done directly in decode/getMetadata
    return true;
}

bool MagickDecoder::close() {
    return true;
}

bool MagickDecoder::decode(const uint8_t* buffer, size_t size, ImageData& output) {
    if (!buffer || size < 12) {
        error_ = "Buffer too small or null";
        return false;
    }

    ImageInfo* info = static_cast<ImageInfo*>(imageInfo_);
    ExceptionInfo* exception = static_cast<ExceptionInfo*>(exceptionInfo_);

    // Reset ImageInfo
    GetImageInfo(info);

    // Set up blob for reading from memory
    info->blob = (void*)buffer;
    info->length = size;

    // Read image
    Image* image = ReadImage(info, exception);
    if (!image || exception->severity != UndefinedException) {
        if (exception->severity != UndefinedException) {
            error_ = std::string("ImageMagick decode error: ") + exception->reason;
        } else {
            error_ = "Failed to decode image with ImageMagick";
        }
        if (image) {
            DestroyImage(image);
        }
        return false;
    }

    // Ensure image is in RGB/RGBA format
    if (image->alpha_trait != UndefinedPixelTrait) {
        (void)SetImageAlpha(image, OpaqueAlpha, exception);
    }

    // Coerce to RGB/RGBA
    if (SetImageType(image, TrueColorType, exception) == MagickFalse) {
        error_ = "Failed to convert image to RGB";
        DestroyImage(image);
        return false;
    }

    // Get image properties
    int width = static_cast<int>(image->columns);
    int height = static_cast<int>(image->rows);
    int channels = (image->alpha_trait != UndefinedPixelTrait) ? 4 : 3;
    int bitsPerChannel = (image->depth > 8) ? 16 : 8;

    // Allocate output buffer
    size_t dataSize = (size_t)width * height * channels * (bitsPerChannel / 8);
    output.data.resize(dataSize);
    output.width = width;
    output.height = height;
    output.bitsPerChannel = bitsPerChannel;
    output.channels = channels;
    output.hasAlpha = (channels == 4);
    output.colorSpace = "sRGB";
    output.format = image->magick;

    // Export pixels row by row
    if (bitsPerChannel == 8) {
        char* pixelData = reinterpret_cast<char*>(output.data.data());
        for (int y = 0; y < height; y++) {
            Quantum* pixels = GetAuthenticPixels(image, 0, y, width, 1, exception);
            if (!pixels) {
                DestroyImage(image);
                error_ = "Failed to read pixel row";
                return false;
            }
            for (int x = 0; x < width; x++) {
                size_t idx = (y * width + x) * channels;
                pixelData[idx + 0] = static_cast<char>(GetPixelRed(image, pixels));
                pixelData[idx + 1] = static_cast<char>(GetPixelGreen(image, pixels));
                pixelData[idx + 2] = static_cast<char>(GetPixelBlue(image, pixels));
                if (channels == 4) {
                    pixelData[idx + 3] = static_cast<char>(GetPixelAlpha(image, pixels));
                }
                pixels += channels;
            }
        }
    } else {
        uint16_t* pixelData = reinterpret_cast<uint16_t*>(output.data.data());
        for (int y = 0; y < height; y++) {
            Quantum* pixels = GetAuthenticPixels(image, 0, y, width, 1, exception);
            if (!pixels) {
                DestroyImage(image);
                error_ = "Failed to read pixel row";
                return false;
            }
            for (int x = 0; x < width; x++) {
                size_t idx = (y * width + x) * channels;
                pixelData[idx + 0] = static_cast<uint16_t>(GetPixelRed(image, pixels));
                pixelData[idx + 1] = static_cast<uint16_t>(GetPixelGreen(image, pixels));
                pixelData[idx + 2] = static_cast<uint16_t>(GetPixelBlue(image, pixels));
                if (channels == 4) {
                    pixelData[idx + 3] = static_cast<uint16_t>(GetPixelAlpha(image, pixels));
                }
                pixels += channels;
            }
        }
    }

    // Sync changes
    (void)SyncAuthenticPixels(image, exception);

    // Extract basic metadata
    output.orientation = static_cast<int>(image->orientation);
    if (output.orientation < 1 || output.orientation > 8) {
        output.orientation = 1;
    }

    // Free image
    DestroyImage(image);

    return true;
}

bool MagickDecoder::getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) {
    if (!buffer || size < 12) {
        error_ = "Buffer too small or null";
        return false;
    }

    ImageInfo* info = static_cast<ImageInfo*>(imageInfo_);
    ExceptionInfo* exception = static_cast<ExceptionInfo*>(exceptionInfo_);

    GetImageInfo(info);

    // Set up blob for reading from memory
    info->blob = (void*)buffer;
    info->length = size;

    // Use PingImage to read header only (no pixel data)
    Image* image = ReadImage(info, exception);
    if (!image || exception->severity != UndefinedException) {
        if (exception->severity != UndefinedException) {
            error_ = std::string("ImageMagick ping error: ") + exception->reason;
        } else {
            error_ = "Failed to ping image with ImageMagick";
        }
        if (image) {
            DestroyImage(image);
        }
        return false;
    }

    // Extract metadata
    metadata.width = static_cast<int>(image->columns);
    metadata.height = static_cast<int>(image->rows);
    metadata.hasAlpha = (image->alpha_trait != UndefinedPixelTrait);
    metadata.bitsPerSample = static_cast<int>(image->depth);
    metadata.colorSpace = "sRGB";
    metadata.orientation = static_cast<int>(image->orientation);
    if (metadata.orientation < 1 || metadata.orientation > 8) {
        metadata.orientation = 1;
    }

    // Get format from magick string
    metadata.format = image->magick;

    DestroyImage(image);
    return true;
}

bool MagickDecoder::extractMetadataOnly(ImageMetadata& metadata) {
    return true;
}

bool MagickDecoder::processImage(ImageData& output) {
    return true;
}

bool MagickDecoder::supportsFormat(ImageFormat format) const {
    return true;
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
    if (format == "pefr" || format == "pef") return ImageFormat::PEFR;
    if (format == "srw") return ImageFormat::SRW;
    if (format == "rwl") return ImageFormat::RWL;
    if (format == "crw") return ImageFormat::CRW;
    if (format == "mrw") return ImageFormat::MRW;
    if (format == "nrw") return ImageFormat::NRW;
    if (format == "erf") return ImageFormat::ERF;
    if (format == "3fr") return ImageFormat::F3FR;
    if (format == "k25") return ImageFormat::K25;
    if (format == "kdc") return ImageFormat::KDC;
    if (format == "mef") return ImageFormat::MEF;
    if (format == "mos") return ImageFormat::MOS;
    if (format == "mraw") return ImageFormat::MRAW;
    if (format == "rrf") return ImageFormat::RRF;
    if (format == "sr2") return ImageFormat::SR2;
    if (format == "rwz") return ImageFormat::RWZ;
    if (format == "heic") return ImageFormat::HEIC;
    if (format == "heif") return ImageFormat::HEIF;
    if (format == "avif") return ImageFormat::AVIF;
    if (format == "jpeg" || format == "jpg" || format == "jpe") return ImageFormat::JPEG;
    if (format == "png") return ImageFormat::PNG;
    if (format == "tiff" || format == "tif") return ImageFormat::TIFF;
    if (format == "webp") return ImageFormat::WEBP;
    if (format == "gif") return ImageFormat::GIF;
    if (format == "bmp") return ImageFormat::BMP;
    if (format == "jxl") return ImageFormat::JXL;
    if (format == "jp2" || format == "j2k") return ImageFormat::JP2;
    if (format == "psd") return ImageFormat::PSD;
    if (format == "pdf") return ImageFormat::PDF;
    if (format == "svg") return ImageFormat::SVG;
    if (format == "ai") return ImageFormat::AI;
    if (format == "eps") return ImageFormat::EPS;
    if (format == "epdf") return ImageFormat::EPDF;
    if (format == "epi") return ImageFormat::EPI;
    if (format == "epsf") return ImageFormat::EPSF;
    if (format == "xps") return ImageFormat::XPS;
    if (format == "exr") return ImageFormat::EXR;
    if (format == "hdr") return ImageFormat::HDR;
    if (format == "bigtiff" || format == "btf") return ImageFormat::BIGTIFF;
    if (format == "cin") return ImageFormat::CIN;
    if (format == "dpx") return ImageFormat::DPX;
    if (format == "fits" || format == "fts" || format == "ftc") return ImageFormat::FITS;
    if (format == "flif") return ImageFormat::FLIF;
    if (format == "miff") return ImageFormat::MIFF;
    if (format == "mpc") return ImageFormat::MPC;
    if (format == "pcd") return ImageFormat::PCD;
    if (format == "pfm") return ImageFormat::PFM;
    if (format == "pict" || format == "pct") return ImageFormat::PICT;
    if (format == "ppm" || format == "pnm") return ImageFormat::PPM;
    if (format == "psb") return ImageFormat::PSB;
    if (format == "psp") return ImageFormat::PSP;
    if (format == "sgi" || format == "rgb" || format == "bw") return ImageFormat::SGI;
    if (format == "tga" || format == "targa") return ImageFormat::TGA;
    if (format == "vtf") return ImageFormat::VTF;
    if (format == "bmp2") return ImageFormat::BMP2;
    if (format == "bmp3") return ImageFormat::BMP3;
    if (format == "dib") return ImageFormat::DIB;
    if (format == "pjpeg") return ImageFormat::PJPEG;
    if (format == "png8") return ImageFormat::PNG8;
    if (format == "png24") return ImageFormat::PNG24;
    if (format == "png32") return ImageFormat::PNG32;
    if (format == "gif87") return ImageFormat::GIF87;
    if (format == "gray") return ImageFormat::GRAY;
    if (format == "graya") return ImageFormat::GRAYA;
    if (format == "cmyk") return ImageFormat::CMYK;
    if (format == "cmyka") return ImageFormat::CMYKA;
    if (format == "pcx") return ImageFormat::PCX;
    if (format == "dcx") return ImageFormat::DCX;
    if (format == "sun" || format == "ras") return ImageFormat::SUN;
    if (format == "ras") return ImageFormat::RAS;
    if (format == "rgb565") return ImageFormat::RGB565;
    if (format == "rgba") return ImageFormat::RGBA;
    if (format == "rgba555") return ImageFormat::RGBA555;
    if (format == "rgb") return ImageFormat::RGB;
    if (format == "pbm") return ImageFormat::PBM;
    if (format == "pgm") return ImageFormat::PGM;
    if (format == "xbm") return ImageFormat::XBM;
    if (format == "xpm") return ImageFormat::XPM;
    if (format == "xwd") return ImageFormat::XWD;
    if (format == "wbmp") return ImageFormat::WBMP;
    if (format == "avs") return ImageFormat::AVS;
    if (format == "dds") return ImageFormat::DDS;
    if (format == "djvu" || format == "djv") return ImageFormat::DJVU;
    if (format == "svgz") return ImageFormat::SVGZ;
    if (format == "wmf") return ImageFormat::WMF;
    if (format == "emf") return ImageFormat::EMF;
    if (format == "cur") return ImageFormat::CUR;
    if (format == "ico") return ImageFormat::ICO;
    if (format == "avi") return ImageFormat::AVI;
    if (format == "mpg" || format == "mpeg") return ImageFormat::MPG;
    if (format == "mov") return ImageFormat::MOV;
    if (format == "mp4" || format == "m4v") return ImageFormat::MP4;
    if (format == "wmv") return ImageFormat::WMV;
    if (format == "flv") return ImageFormat::FLV;
    if (format == "mkv") return ImageFormat::MKV;
    if (format == "mng") return ImageFormat::MNG;
    if (format == "jng") return ImageFormat::JNG;
    if (format == "mpo") return ImageFormat::MPO;
    if (format == "doc") return ImageFormat::DOC;
    if (format == "docx") return ImageFormat::DOCX;
    if (format == "xls") return ImageFormat::XLS;
    if (format == "xlsx") return ImageFormat::XLSX;
    if (format == "ppt") return ImageFormat::PPT;
    if (format == "pptx") return ImageFormat::PPTX;
    if (format == "aai") return ImageFormat::AAI;
    if (format == "art") return ImageFormat::ART;
    if (format == "blp") return ImageFormat::BLP;
    if (format == "brf") return ImageFormat::BRF;
    if (format == "cals") return ImageFormat::CALS;
    if (format == "caption") return ImageFormat::CAPTION;
    if (format == "clip") return ImageFormat::CLIP;
    if (format == "dcm") return ImageFormat::DCM;
    if (format == "dot") return ImageFormat::DOT;
    if (format == "dpr") return ImageFormat::DPR;
    if (format == "font" || format == "fnt") return ImageFormat::FONT;
    if (format == "fpx") return ImageFormat::FPX;
    if (format == "fractal") return ImageFormat::FRACTAL;
    if (format == "gradient") return ImageFormat::GRADIENT;
    if (format == "h") return ImageFormat::H;
    if (format == "hald") return ImageFormat::HALD;
    if (format == "hcl") return ImageFormat::HCL;
    if (format == "histogram") return ImageFormat::HISTOGRAM;
    if (format == "hrz") return ImageFormat::HRZ;
    if (format == "htm" || format == "html") return ImageFormat::HTML;
    if (format == "icb") return ImageFormat::ICB;
    if (format == "icc") return ImageFormat::ICC;
    if (format == "icer") return ImageFormat::ICER;
    if (format == "info") return ImageFormat::INFO;
    if (format == "inline") return ImageFormat::INLINE;
    if (format == "ipl") return ImageFormat::IPL;
    if (format == "isobrl") return ImageFormat::ISOBRL;
    if (format == "jbg") return ImageFormat::JBG;
    if (format == "jnx") return ImageFormat::JNX;
    if (format == "jpm") return ImageFormat::JPM;
    if (format == "jps") return ImageFormat::JPS;
    if (format == "jpx") return ImageFormat::JPX;
    if (format == "k") return ImageFormat::K;
    if (format == "kvec") return ImageFormat::KVEC;
    if (format == "label") return ImageFormat::LABEL;
    if (format == "linux") return ImageFormat::LINUX;
    if (format == "mac") return ImageFormat::MAC;
    if (format == "map") return ImageFormat::MAP;
    if (format == "mat") return ImageFormat::MAT;
    if (format == "matte") return ImageFormat::MATTE;
    if (format == "mono") return ImageFormat::MONO;
    if (format == "msl") return ImageFormat::MSL;
    if (format == "mtv") return ImageFormat::MTV;
    if (format == "mvg") return ImageFormat::MVG;
    if (format == "null") return ImageFormat::NULLIMG;
    if (format == "ora") return ImageFormat::ORA;
    if (format == "otb") return ImageFormat::OTB;
    if (format == "pal") return ImageFormat::PAL;
    if (format == "palm") return ImageFormat::PALM;
    if (format == "pcds") return ImageFormat::PCDS;
    if (format == "pdb") return ImageFormat::PDB;
    if (format == "pfa") return ImageFormat::PFA;
    if (format == "pfb") return ImageFormat::PFB;
    if (format == "picon") return ImageFormat::PICON;
    if (format == "pix") return ImageFormat::PIX;
    if (format == "plasma") return ImageFormat::PLASMA;
    if (format == "pocketmod") return ImageFormat::POCKETMOD;
    if (format == "ps") return ImageFormat::PS;
    if (format == "ps2") return ImageFormat::PS2;
    if (format == "ps3") return ImageFormat::PS3;
    if (format == "ptif") return ImageFormat::PTIF;
    if (format == "pwp") return ImageFormat::PWP;
    if (format == "rgf") return ImageFormat::RGF;
    if (format == "rla") return ImageFormat::RLA;
    if (format == "rpm") return ImageFormat::RPM;
    if (format == "rsle") return ImageFormat::RSLE;
    if (format == "sfw") return ImageFormat::SFW;
    if (format == "shtml") return ImageFormat::SHTML;
    if (format == "stream") return ImageFormat::STREAM;
    if (format == "text") return ImageFormat::TEXT;
    if (format == "tile") return ImageFormat::TILE;
    if (format == "tim") return ImageFormat::TIM;
    if (format == "tm2") return ImageFormat::TM2;
    if (format == "ttc") return ImageFormat::TTC;
    if (format == "ttf") return ImageFormat::TTF;
    if (format == "txt") return ImageFormat::TXT;
    if (format == "ubrl") return ImageFormat::UBRL;
    if (format == "ubrl6") return ImageFormat::UBRL6;
    if (format == "uil") return ImageFormat::UIL;
    if (format == "uyvy") return ImageFormat::UYVY;
    if (format == "vda") return ImageFormat::VDA;
    if (format == "viff" || format == "vif") return ImageFormat::VIFF;
    if (format == "vips") return ImageFormat::VIPS;
    if (format == "wdp") return ImageFormat::WDP;
    if (format == "wing") return ImageFormat::WING;
    if (format == "wpg") return ImageFormat::WPG;
    if (format == "x") return ImageFormat::X;
    if (format == "xc") return ImageFormat::XC;
    if (format == "xcf") return ImageFormat::XCF;
    if (format == "xv") return ImageFormat::XV;
    if (format == "yuv") return ImageFormat::YUV;
    if (format == "yuva") return ImageFormat::YUVA;
    if (format == "g") return ImageFormat::G;
    if (format == "g3") return ImageFormat::G3;
    if (format == "g4") return ImageFormat::G4;
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
        case ImageFormat::CRW: return "crw";
        case ImageFormat::MRW: return "mrw";
        case ImageFormat::NRW: return "nrw";
        case ImageFormat::PEF: return "pef";
        case ImageFormat::ERF: return "erf";
        case ImageFormat::GRB: return "grb";
        case ImageFormat::F3FR: return "3fr";
        case ImageFormat::K25: return "k25";
        case ImageFormat::KDC: return "kdc";
        case ImageFormat::MEF: return "mef";
        case ImageFormat::MOS: return "mos";
        case ImageFormat::MRAW: return "mraw";
        case ImageFormat::RRF: return "rrf";
        case ImageFormat::SR2: return "sr2";
        case ImageFormat::RWZ: return "rwz";
        case ImageFormat::HEIC: return "heic";
        case ImageFormat::HEIF: return "heif";
        case ImageFormat::AVIF: return "avif";
        case ImageFormat::JPEG: return "jpeg";
        case ImageFormat::PNG: return "png";
        case ImageFormat::TIFF: return "tiff";
        case ImageFormat::WEBP: return "webp";
        case ImageFormat::GIF: return "gif";
        case ImageFormat::BMP: return "bmp";
        case ImageFormat::JXL: return "jxl";
        case ImageFormat::JP2: return "jp2";
        case ImageFormat::PSD: return "psd";
        case ImageFormat::PDF: return "pdf";
        case ImageFormat::SVG: return "svg";
        case ImageFormat::AI: return "ai";
        case ImageFormat::DOC: return "doc";
        case ImageFormat::DOCX: return "docx";
        case ImageFormat::XLS: return "xls";
        case ImageFormat::XLSX: return "xlsx";
        case ImageFormat::PPT: return "ppt";
        case ImageFormat::PPTX: return "pptx";
        case ImageFormat::EPS: return "eps";
        case ImageFormat::EPDF: return "epdf";
        case ImageFormat::EPI: return "epi";
        case ImageFormat::EPSF: return "epsf";
        case ImageFormat::EPS2: return "eps2";
        case ImageFormat::EPS3: return "eps3";
        case ImageFormat::EPSI: return "epsi";
        case ImageFormat::XPS: return "xps";
        case ImageFormat::EXR: return "exr";
        case ImageFormat::HDR: return "hdr";
        case ImageFormat::BIGTIFF: return "bigtiff";
        case ImageFormat::CIN: return "cin";
        case ImageFormat::DPX: return "dpx";
        case ImageFormat::FITS: return "fits";
        case ImageFormat::FLIF: return "flif";
        case ImageFormat::J2K: return "j2k";
        case ImageFormat::MIFF: return "miff";
        case ImageFormat::MPC: return "mpc";
        case ImageFormat::PCD: return "pcd";
        case ImageFormat::PFM: return "pfm";
        case ImageFormat::PICT: return "pict";
        case ImageFormat::PPM: return "ppm";
        case ImageFormat::PSB: return "psb";
        case ImageFormat::PSP: return "psp";
        case ImageFormat::SGI: return "sgi";
        case ImageFormat::TGA: return "tga";
        case ImageFormat::VTF: return "vtf";
        case ImageFormat::BMP2: return "bmp2";
        case ImageFormat::BMP3: return "bmp3";
        case ImageFormat::DIB: return "dib";
        case ImageFormat::PJPEG: return "pjpeg";
        case ImageFormat::PNG8: return "png8";
        case ImageFormat::PNG24: return "png24";
        case ImageFormat::PNG32: return "png32";
        case ImageFormat::GIF87: return "gif87";
        case ImageFormat::GRAY: return "gray";
        case ImageFormat::GRAYA: return "graya";
        case ImageFormat::CMYK: return "cmyk";
        case ImageFormat::CMYKA: return "cmyka";
        case ImageFormat::PCX: return "pcx";
        case ImageFormat::DCX: return "dcx";
        case ImageFormat::SUN: return "sun";
        case ImageFormat::RAS: return "ras";
        case ImageFormat::RGB565: return "rgb565";
        case ImageFormat::RGBA: return "rgba";
        case ImageFormat::RGBA555: return "rgba555";
        case ImageFormat::RGB: return "rgb";
        case ImageFormat::PBM: return "pbm";
        case ImageFormat::PGM: return "pgm";
        case ImageFormat::XBM: return "xbm";
        case ImageFormat::XPM: return "xpm";
        case ImageFormat::XWD: return "xwd";
        case ImageFormat::WBMP: return "wbmp";
        case ImageFormat::AVS: return "avs";
        case ImageFormat::DDS: return "dds";
        case ImageFormat::DJVU: return "djvu";
        case ImageFormat::SVGZ: return "svgz";
        case ImageFormat::WMF: return "wmf";
        case ImageFormat::EMF: return "emf";
        case ImageFormat::CUR: return "cur";
        case ImageFormat::ICO: return "ico";
        case ImageFormat::AVI: return "avi";
        case ImageFormat::MPG: return "mpg";
        case ImageFormat::MPEG: return "mpeg";
        case ImageFormat::MOV: return "mov";
        case ImageFormat::MP4: return "mp4";
        case ImageFormat::M4V: return "m4v";
        case ImageFormat::WMV: return "wmv";
        case ImageFormat::FLV: return "flv";
        case ImageFormat::MKV: return "mkv";
        case ImageFormat::MNG: return "mng";
        case ImageFormat::JNG: return "jng";
        case ImageFormat::MPO: return "mpo";
        case ImageFormat::AAI: return "aai";
        case ImageFormat::ART: return "art";
        case ImageFormat::BLP: return "blp";
        case ImageFormat::BRF: return "brf";
        case ImageFormat::CALS: return "cals";
        case ImageFormat::CAPTION: return "caption";
        case ImageFormat::CLIP: return "clip";
        case ImageFormat::DCM: return "dcm";
        case ImageFormat::DOT: return "dot";
        case ImageFormat::DPR: return "dpr";
        case ImageFormat::FONT: return "font";
        case ImageFormat::FPX: return "fpx";
        case ImageFormat::FRACTAL: return "fractal";
        case ImageFormat::G: return "g";
        case ImageFormat::G3: return "g3";
        case ImageFormat::G4: return "g4";
        case ImageFormat::GRADIENT: return "gradient";
        case ImageFormat::GRBO: return "grbo";
        case ImageFormat::H: return "h";
        case ImageFormat::HALD: return "hald";
        case ImageFormat::HCL: return "hcl";
        case ImageFormat::HISTOGRAM: return "histogram";
        case ImageFormat::HRZ: return "hrz";
        case ImageFormat::HTML: return "html";
        case ImageFormat::HTM: return "htm";
        case ImageFormat::ICB: return "icb";
        case ImageFormat::ICC: return "icc";
        case ImageFormat::ICER: return "icer";
        case ImageFormat::INFO: return "info";
        case ImageFormat::INLINE: return "inline";
        case ImageFormat::IPL: return "ipl";
        case ImageFormat::ISOBRL: return "isobrl";
        case ImageFormat::ISOBRL6: return "isobrl6";
        case ImageFormat::JBG: return "jbg";
        case ImageFormat::JPE: return "jpe";
        case ImageFormat::JPM: return "jpm";
        case ImageFormat::JPS: return "jps";
        case ImageFormat::JPX: return "jpx";
        case ImageFormat::JNX: return "jnx";
        case ImageFormat::K: return "k";
        case ImageFormat::KVEC: return "kvec";
        case ImageFormat::LABEL: return "label";
        case ImageFormat::LINUX: return "linux";
        case ImageFormat::MAC: return "mac";
        case ImageFormat::MAP: return "map";
        case ImageFormat::MAT: return "mat";
        case ImageFormat::MATTE: return "matte";
        case ImageFormat::MONO: return "mono";
        case ImageFormat::MSL: return "msl";
        case ImageFormat::MTV: return "mtv";
        case ImageFormat::MVG: return "mvg";
        case ImageFormat::NULLIMG: return "null";
        case ImageFormat::ORA: return "ora";
        case ImageFormat::OTB: return "otb";
        case ImageFormat::PAL: return "pal";
        case ImageFormat::PALM: return "palm";
        case ImageFormat::PCDS: return "pcds";
        case ImageFormat::PDB: return "pdb";
        case ImageFormat::PFA: return "pfa";
        case ImageFormat::PFB: return "pfb";
        case ImageFormat::PICON: return "picon";
        case ImageFormat::PIX: return "pix";
        case ImageFormat::PLASMA: return "plasma";
        case ImageFormat::POCKETMOD: return "pocketmod";
        case ImageFormat::PS: return "ps";
        case ImageFormat::PS2: return "ps2";
        case ImageFormat::PS3: return "ps3";
        case ImageFormat::PTIF: return "ptif";
        case ImageFormat::PWP: return "pwp";
        case ImageFormat::RGF: return "rgf";
        case ImageFormat::RLA: return "rla";
        case ImageFormat::RLE: return "rle";
        case ImageFormat::RPM: return "rpm";
        case ImageFormat::RSLE: return "rsle";
        case ImageFormat::SFW: return "sfw";
        case ImageFormat::SHTML: return "shtml";
        case ImageFormat::STREAM: return "stream";
        case ImageFormat::TEXT: return "text";
        case ImageFormat::TILE: return "tile";
        case ImageFormat::TIM: return "tim";
        case ImageFormat::TM2: return "tm2";
        case ImageFormat::TTC: return "ttc";
        case ImageFormat::TTF: return "ttf";
        case ImageFormat::TXT: return "txt";
        case ImageFormat::UBRL: return "ubrl";
        case ImageFormat::UBRL6: return "ubrl6";
        case ImageFormat::UIL: return "uil";
        case ImageFormat::UYVY: return "uyvy";
        case ImageFormat::VDA: return "vda";
        case ImageFormat::VIFF: return "viff";
        case ImageFormat::VIPS: return "vips";
        case ImageFormat::WDP: return "wdp";
        case ImageFormat::WING: return "wing";
        case ImageFormat::WPG: return "wpg";
        case ImageFormat::X: return "x";
        case ImageFormat::XC: return "xc";
        case ImageFormat::XCF: return "xcf";
        case ImageFormat::XV: return "xv";
        case ImageFormat::YUV: return "yuv";
        case ImageFormat::YUVA: return "yuva";
        case ImageFormat::UNKNOWN: return "unknown";
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
        case ImageFormat::CRW: return "image/x-canon-crw";
        case ImageFormat::MRW: return "image/x-minolta-mrw";
        case ImageFormat::NRW: return "image/x-nikon-nrw";
        case ImageFormat::PEF: return "image/x-pentax-pef";
        case ImageFormat::ERF: return "image/x-epson-erf";
        case ImageFormat::MEF: return "image/x-mamiya-mef";
        case ImageFormat::MOS: return "image/x-leaf-mos";
        case ImageFormat::HEIC: return "image/heic";
        case ImageFormat::HEIF: return "image/heif";
        case ImageFormat::AVIF: return "image/avif";
        case ImageFormat::JPEG: return "image/jpeg";
        case ImageFormat::PNG: return "image/png";
        case ImageFormat::TIFF: return "image/tiff";
        case ImageFormat::WEBP: return "image/webp";
        case ImageFormat::GIF: return "image/gif";
        case ImageFormat::BMP: return "image/bmp";
        case ImageFormat::JXL: return "image/jxl";
        case ImageFormat::JP2: return "image/jp2";
        case ImageFormat::PSD: return "image/vnd.adobe.photoshop";
        case ImageFormat::PDF: return "application/pdf";
        case ImageFormat::SVG: return "image/svg+xml";
        case ImageFormat::AI: return "application/illustrator";
        case ImageFormat::DOC: return "application/msword";
        case ImageFormat::DOCX: return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        case ImageFormat::XLS: return "application/vnd.ms-excel";
        case ImageFormat::XLSX: return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        case ImageFormat::PPT: return "application/vnd.ms-powerpoint";
        case ImageFormat::PPTX: return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
        case ImageFormat::EPS: return "application/postscript";
        case ImageFormat::EPDF: return "application/pdf";
        case ImageFormat::XPS: return "application/xps";
        case ImageFormat::EXR: return "image/x-exr";
        case ImageFormat::HDR: return "image/vnd.radiance";
        case ImageFormat::BIGTIFF: return "image/tiff";
        case ImageFormat::CIN: return "image/x-cin";
        case ImageFormat::DPX: return "image/x-dpx";
        case ImageFormat::FITS: return "image/x-fits";
        case ImageFormat::FLIF: return "image/x-flif";
        case ImageFormat::MIFF: return "application/x-magick-image";
        case ImageFormat::MPC: return "application/x-magick-persistent-cache";
        case ImageFormat::PCD: return "image/x-photo-cd";
        case ImageFormat::PFM: return "image/x-portable-floatmap";
        case ImageFormat::PICT: return "image/x-pict";
        case ImageFormat::PPM: return "image/x-portable-pixmap";
        case ImageFormat::SGI: return "image/x-sgi";
        case ImageFormat::TGA: return "image/x-tga";
        case ImageFormat::BMP2: return "image/bmp";
        case ImageFormat::BMP3: return "image/bmp";
        case ImageFormat::DIB: return "image/bmp";
        case ImageFormat::GRAY: return "image/x-raw-gray";
        case ImageFormat::GRAYA: return "image/x-raw-graya";
        case ImageFormat::CMYK: return "image/x-cmyk";
        case ImageFormat::CMYKA: return "image/x-cmyka";
        case ImageFormat::PCX: return "image/x-pcx";
        case ImageFormat::DCX: return "image/x-dcx";
        case ImageFormat::SUN: return "image/x-sun-raster";
        case ImageFormat::RAS: return "image/x-cmu-raster";
        case ImageFormat::RGB: return "image/x-rgb";
        case ImageFormat::RGBA: return "image/x-rgba";
        case ImageFormat::PBM: return "image/x-portable-bitmap";
        case ImageFormat::PGM: return "image/x-portable-graymap";
        case ImageFormat::XBM: return "image/x-xbitmap";
        case ImageFormat::XPM: return "image/x-xpixmap";
        case ImageFormat::XWD: return "image/x-xwindowdump";
        case ImageFormat::WBMP: return "image/vnd.wap.wbmp";
        case ImageFormat::DDS: return "image/x-dds";
        case ImageFormat::DJVU: return "image/vnd.djvu";
        case ImageFormat::SVGZ: return "image/svg+xml-compressed";
        case ImageFormat::WMF: return "application/x-wmf";
        case ImageFormat::EMF: return "application/x-emf";
        case ImageFormat::CUR: return "image/x-win-bitmap";
        case ImageFormat::ICO: return "image/x-icon";
        case ImageFormat::AVI: return "video/x-msvideo";
        case ImageFormat::MPG: return "video/mpeg";
        case ImageFormat::MPEG: return "video/mpeg";
        case ImageFormat::MOV: return "video/quicktime";
        case ImageFormat::MP4: return "video/mp4";
        case ImageFormat::M4V: return "video/x-m4v";
        case ImageFormat::WMV: return "video/x-ms-wmv";
        case ImageFormat::FLV: return "video/x-flv";
        case ImageFormat::MKV: return "video/x-matroska";
        case ImageFormat::MNG: return "video/x-mng";
        case ImageFormat::JNG: return "image/x-jng";
        case ImageFormat::MPO: return "image/x-mpo";
        case ImageFormat::DCM: return "application/dicom";
        case ImageFormat::PSB: return "image/vnd.adobe.photoshop";
        case ImageFormat::PSP: return "image/x-psp";
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
        case ImageFormat::PEF: return "pef";
        case ImageFormat::SRW: return "srw";
        case ImageFormat::RWL: return "rwl";
        case ImageFormat::CRW: return "crw";
        case ImageFormat::MRW: return "mrw";
        case ImageFormat::NRW: return "nrw";
        case ImageFormat::ERF: return "erf";
        case ImageFormat::MEF: return "mef";
        case ImageFormat::MOS: return "mos";
        case ImageFormat::HEIC: return "heic";
        case ImageFormat::HEIF: return "heif";
        case ImageFormat::AVIF: return "avif";
        case ImageFormat::JPEG: return "jpg";
        case ImageFormat::PNG: return "png";
        case ImageFormat::TIFF: return "tiff";
        case ImageFormat::WEBP: return "webp";
        case ImageFormat::GIF: return "gif";
        case ImageFormat::BMP: return "bmp";
        case ImageFormat::JXL: return "jxl";
        case ImageFormat::JP2: return "jp2";
        case ImageFormat::PSD: return "psd";
        case ImageFormat::PDF: return "pdf";
        case ImageFormat::SVG: return "svg";
        case ImageFormat::AI: return "ai";
        case ImageFormat::DOC: return "doc";
        case ImageFormat::DOCX: return "docx";
        case ImageFormat::XLS: return "xls";
        case ImageFormat::XLSX: return "xlsx";
        case ImageFormat::PPT: return "ppt";
        case ImageFormat::PPTX: return "pptx";
        case ImageFormat::EPS: return "eps";
        case ImageFormat::EPDF: return "epdf";
        case ImageFormat::XPS: return "xps";
        case ImageFormat::EXR: return "exr";
        case ImageFormat::HDR: return "hdr";
        case ImageFormat::BIGTIFF: return "tiff";
        case ImageFormat::CIN: return "cin";
        case ImageFormat::DPX: return "dpx";
        case ImageFormat::FITS: return "fits";
        case ImageFormat::FLIF: return "flif";
        case ImageFormat::MIFF: return "miff";
        case ImageFormat::MPC: return "mpc";
        case ImageFormat::PCD: return "pcd";
        case ImageFormat::PFM: return "pfm";
        case ImageFormat::PICT: return "pict";
        case ImageFormat::PPM: return "ppm";
        case ImageFormat::SGI: return "sgi";
        case ImageFormat::TGA: return "tga";
        case ImageFormat::GRAY: return "gray";
        case ImageFormat::GRAYA: return "graya";
        case ImageFormat::CMYK: return "cmyk";
        case ImageFormat::CMYKA: return "cmyka";
        case ImageFormat::PCX: return "pcx";
        case ImageFormat::DCX: return "dcx";
        case ImageFormat::SUN: return "sun";
        case ImageFormat::RAS: return "ras";
        case ImageFormat::RGB: return "rgb";
        case ImageFormat::RGBA: return "rgba";
        case ImageFormat::PBM: return "pbm";
        case ImageFormat::PGM: return "pgm";
        case ImageFormat::XBM: return "xbm";
        case ImageFormat::XPM: return "xpm";
        case ImageFormat::XWD: return "xwd";
        case ImageFormat::WBMP: return "wbmp";
        case ImageFormat::DDS: return "dds";
        case ImageFormat::DJVU: return "djvu";
        case ImageFormat::SVGZ: return "svgz";
        case ImageFormat::WMF: return "wmf";
        case ImageFormat::EMF: return "emf";
        case ImageFormat::CUR: return "cur";
        case ImageFormat::ICO: return "ico";
        case ImageFormat::AVI: return "avi";
        case ImageFormat::MPG: return "mpg";
        case ImageFormat::MPEG: return "mpeg";
        case ImageFormat::MOV: return "mov";
        case ImageFormat::MP4: return "mp4";
        case ImageFormat::M4V: return "m4v";
        case ImageFormat::WMV: return "wmv";
        case ImageFormat::FLV: return "flv";
        case ImageFormat::MKV: return "mkv";
        case ImageFormat::MNG: return "mng";
        case ImageFormat::JNG: return "jng";
        case ImageFormat::MPO: return "mpo";
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

bool isPSD(const uint8_t* buffer, size_t size) {
    // PSD signature: "8BPS" at offset 0
    if (size < 4) return false;
    return strncmp(reinterpret_cast<const char*>(buffer), "8BPS", 4) == 0;
}

bool isPDF(const uint8_t* buffer, size_t size) {
    // PDF signature: "%PDF-" at offset 0
    if (size < 5) return false;
    return strncmp(reinterpret_cast<const char*>(buffer), "%PDF-", 5) == 0;
}

bool isEXR(const uint8_t* buffer, size_t size) {
    // EXR signature: 0x76, 0x2f, 0x31, 0x01 at offset 0
    if (size < 4) return false;
    return buffer[0] == 0x76 && buffer[1] == 0x2f &&
           buffer[2] == 0x31 && buffer[3] == 0x01;
}

bool isHDR(const uint8_t* buffer, size_t size) {
    // HDR/Radiance signature: "#?RADIANCE" at offset 0
    if (size < 10) return false;
    return strncmp(reinterpret_cast<const char*>(buffer), "#?RADIANCE", 10) == 0;
}

} // namespace ImageFormatUtil
