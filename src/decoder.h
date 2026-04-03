/**
 * nImage - Native Image Decoder
 *
 * High-performance image decoding via NAPI using:
 * - libraw: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
 * - libheif: HEIC/HEIF formats (Apple's modern image format)
 *
 * Features:
 * - Full ICC color profile support
 * - Metadata extraction without full decode
 * - Memory-efficient processing
 * - No external CLI dependencies
 */

#ifndef NIMAGE_DECODER_H
#define NIMAGE_DECODER_H

#include <string>
#include <vector>
#include <cstdint>
#include <libheif/heif.h>

/**
 * Output image data structure
 */
struct ImageData {
    // Pixel data (RGB or RGBA, depending on alpha support)
    std::vector<uint8_t> data;

    // Image dimensions
    int width = 0;
    int height = 0;

    // Color depth (bits per channel)
    int bitsPerChannel = 8;

    // Number of channels (3 = RGB, 4 = RGBA)
    int channels = 3;

    // Color space (sRGB, AdobeRGB, etc.)
    std::string colorSpace;

    // ICC profile data (if available)
    std::vector<uint8_t> iccProfile;

    // Original format
    std::string format;

    // Camera metadata
    std::string cameraMake;
    std::string cameraModel;

    // Capture metadata
    std::string dateTime;
    double exposureTime = 0.0;
    double fNumber = 0.0;
    int isoSpeed = 0;
    double focalLength = 0.0;

    // Dimensions
    int rawWidth = 0;
    int rawHeight = 0;

    // Orientation (1-8, as per EXIF)
    int orientation = 1;

    // Whether the image has an alpha channel
    bool hasAlpha = false;
};

/**
 * Image metadata without full decode
 */
struct ImageMetadata {
    // Detected format
    std::string format;

    // Dimensions
    int width = 0;
    int height = 0;

    // RAW-specific
    int rawWidth = 0;
    int rawHeight = 0;
    std::string cameraMake;
    std::string cameraModel;
    int isoSpeed = 0;
    double exposureTime = 0.0;
    double fNumber = 0.0;
    double focalLength = 0.0;

    // Color
    std::string colorSpace;
    bool hasAlpha = false;
    int bitsPerSample = 0;

    // Orientation
    int orientation = 1;

    // File info
    size_t fileSize = 0;

    // Lens info
    std::string lensModel;
};

/**
 * Supported image formats
 */
enum class ImageFormat {
    UNKNOWN = 0,
    // RAW formats
    CR2,    // Canon RAW 2
    NEF,    // Nikon Electronic Format
    ARW,    // Sony Alpha RAW
    ORF,    // Olympus RAW Format
    RAF,    // Fujifilm RAW Format
    RW2,    // Panasonic RAW
    DNG,    // Adobe Digital Negative
    PEFR,   // Pentax RAW
    SRW,    // Samsung RAW
    RWL,    // Leica RAW
    // HEIC/HEIF
    HEIC,   // High Efficiency Image Format (Apple)
    HEIF,   // High Efficiency Image Format
    AVIF,   // AV1 Image Format
    // Standard
    JPEG,
    PNG,
    TIFF,
    WEBP,
    GIF,
    // ImageMagick-handled formats
    PSD,    // Photoshop Document
    PDF,    // PDF Document (rasterized)
    SVG,    // SVG Vector (rasterized)
    EXR,    // OpenEXR
    HDR,    // Radiance HDR
    BIGTIFF // BigTIFF (>4GB)
};

/**
 * Detection result with format confidence
 */
struct FormatDetection {
    ImageFormat format = ImageFormat::UNKNOWN;
    float confidence = 0.0f;  // 0.0 - 1.0
    std::string mimeType;
};

/**
 * Base class for image decoders
 */
class ImageDecoder {
public:
    ImageDecoder() = default;
    virtual ~ImageDecoder() = default;

    // Non-copyable
    ImageDecoder(const ImageDecoder&) = delete;
    ImageDecoder& operator=(const ImageDecoder&) = delete;

    // Factory method to create appropriate decoder
    static ImageDecoder* createDecoder(ImageFormat format);
    static ImageDecoder* createDecoderForBuffer(const uint8_t* buffer, size_t size);

    // Detect format from buffer
    static FormatDetection detectFormat(const uint8_t* buffer, size_t size);

    // Decode image to RGB/RGBA
    virtual bool decode(const uint8_t* buffer, size_t size, ImageData& output) = 0;

    // Get metadata without full decode
    virtual bool getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) = 0;

    // Check if this decoder supports the given format
    virtual bool supportsFormat(ImageFormat format) const = 0;

    // Get format name
    virtual const char* formatName() const = 0;

    // Get last error message
    const std::string& getError() const { return error_; }

protected:
    std::string error_;
};

/**
 * LibRaw decoder for RAW formats
 */
class LibRawDecoder : public ImageDecoder {
public:
    LibRawDecoder();
    ~LibRawDecoder() override;

    bool decode(const uint8_t* buffer, size_t size, ImageData& output) override;
    bool getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) override;
    bool supportsFormat(ImageFormat format) const override;
    const char* formatName() const override { return "LibRaw"; }

private:
    // Internal state
    void* libraw_data_;  // Opaque handle to libraw state

    bool openBuffer(const uint8_t* buffer, size_t size);
    void close();

    bool processRAW(ImageData& output);
    bool extractMetadata(ImageMetadata& metadata);
    void extractMetadataToImageData(ImageData& output);
};

/**
 * LibHeif decoder for HEIC/HEIF formats
 */
class LibHeifDecoder : public ImageDecoder {
public:
    LibHeifDecoder();
    ~LibHeifDecoder() override;

    bool decode(const uint8_t* buffer, size_t size, ImageData& output) override;
    bool getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) override;
    bool supportsFormat(ImageFormat format) const override;
    const char* formatName() const override { return "LibHeif"; }

private:
    // Internal state
    heif_context* heif_context_;
    heif_image_handle* heif_handle_;

    bool openBuffer(const uint8_t* buffer, size_t size);
    void close();

    bool decodeHeif(ImageData& output);
    bool extractMetadata(ImageMetadata& metadata);
    void extractMetadataToImageData(ImageData& output);
};

/**
 * MagickDecoder for ImageMagick-handled formats (PSD, PDF, SVG, EXR, HDR, BigTIFF)
 */
class MagickDecoder : public ImageDecoder {
public:
    MagickDecoder();
    ~MagickDecoder() override;

    bool decode(const uint8_t* buffer, size_t size, ImageData& output) override;
    bool getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) override;
    bool supportsFormat(ImageFormat format) const override;
    const char* formatName() const override { return "ImageMagick"; }

private:
    // Internal state
    void* imageInfo_;      // MagickCore ImageInfo*
    void* exceptionInfo_;  // ExceptionInfo*

    bool openBuffer(const uint8_t* buffer, size_t size);
    bool close();
    bool processImage(ImageData& output);
    bool extractMetadataOnly(ImageMetadata& metadata);
};

/**
 * Format utility functions
 */
namespace ImageFormatUtil {
    ImageFormat formatFromString(const std::string& format);
    std::string formatToString(ImageFormat format);
    std::string formatToMimeType(ImageFormat format);
    const char* formatToExtension(ImageFormat format);

    // Check magic bytes
    bool isRAW(const uint8_t* buffer, size_t size);
    bool isHEIC(const uint8_t* buffer, size_t size);
    bool isJPEG(const uint8_t* buffer, size_t size);
    bool isPNG(const uint8_t* buffer, size_t size);
    bool isTIFF(const uint8_t* buffer, size_t size);
    bool isWebP(const uint8_t* buffer, size_t size);
    // ImageMagick format detection
    bool isPSD(const uint8_t* buffer, size_t size);
    bool isPDF(const uint8_t* buffer, size_t size);
    bool isEXR(const uint8_t* buffer, size_t size);
    bool isHDR(const uint8_t* buffer, size_t size);
}

#endif // NIMAGE_DECODER_H
