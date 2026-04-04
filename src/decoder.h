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

    // Tile position (for streaming tiles)
    int x = 0;
    int y = 0;

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
 * Organized by decoder: LibRaw (RAW), LibHeif (HEIC), Sharp (standard), MagickCore (all others)
 * MagickCore is the fallback for any format not handled by the specialized decoders
 */
enum class ImageFormat {
    UNKNOWN = 0,

    // RAW formats (LibRaw)
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
    CRW,    // Canon RAW (older)
    MRW,    // Minolta RAW
    NRW,    // Nikon RAW
    F3FR,   // Hasselblad RAW
    K25,    // Kodak DC25 RAW
    KDC,    // Kodak RAW
    MEF,    // Mamiya RAW
    MOS,    // Leaf RAW
    MRAW,   // Leica RAW (variant)
    PEF,    // Pentax RAW (variant)
    RRF,    // Canon RAW (v2)
    SR2,    // Sony RAW (variant)
    RWZ,    // Rawzor
    ERF,    // Epson RAW
    GRB,    // Gremlin RAW

    // HEIC/HEIF/AVIF (LibHeif)
    HEIC,   // High Efficiency Image Format (Apple)
    HEIF,   // High Efficiency Image Format
    AVIF,   // AV1 Image Format

    // Standard formats (Sharp/libjpeg/libpng/libwebp)
    JPEG,   // Joint Photographic Experts Group
    PNG,    // Portable Network Graphics
    TIFF,   // Tagged Image File Format
    WEBP,   // WebP
    GIF,    // Graphics Interchange Format
    BMP,    // Bitmap
    JXL,    // JPEG XL
    JP2,    // JPEG 2000

    // Document formats (ImageMagick)
    PSD,    // Photoshop Document
    PDF,    // PDF Document
    SVG,    // SVG Vector
    AI,     // Adobe Illustrator
    DOC,    // Microsoft Word
    DOCX,   // Microsoft Word (Office Open XML)
    XLS,    // Microsoft Excel
    XLSX,   // Microsoft Excel (Office Open XML)
    PPT,    // Microsoft PowerPoint
    PPTX,   // Microsoft PowerPoint (Office Open XML)
    EPS,    // Encapsulated PostScript
    EPDF,   // Encapsulated PDF
    EPI,    // Encapsulated PostScript Interchange
    EPSF,   // PostScript
    XPS,    // XPS Document

    // Scientific/imaging formats (ImageMagick)
    EXR,    // OpenEXR
    HDR,    // Radiance HDR
    BIGTIFF,// BigTIFF (>4GB)
    CIN,    // Kodak Cineon
    DPX,    // SMPTE 268M
    FITS,   // Flexible Image Transport System
    FLIF,   // Free Lossless Image Format
    J2K,    // JPEG-2000 Code Stream
    MIFF,   // Magick Image File Format
    MPC,    // Magick Persistent Cache
    PCD,    // Photo CD
    PFM,    // Portable Float Map
    PICT,   // Apple PICT
    PPM,    // Portable Pixel Map
    PSB,    // Photoshop Large Document
    PSP,    // Paint Shop Pro
    SGI,    // Silicon Graphics RGB
    TGA,    // Truevision TGA
    VTF,    // Nintendo VTF

    // Video formats stills (ImageMagick)
    AVI,    // Microsoft AVI
    MPG,    // MPEG-1/2
    MPEG,   // MPEG
    MOV,    // QuickTime
    MP4,    // MPEG-4
    M4V,    // MPEG-4 Video
    WMV,    // Windows Media Video
    WMF,    // Windows Metafile
    FLV,    // Flash Video
    MKV,    // Matroska

    // ImageMagick-specific/internal formats
    AAI,    // AAI Dune
    ADT,    // Adobe Document
    ART,    // Peter M. G. ART
    AVS,    // AVS X image
    BIE,    // Joint Bi-level Image Experts Group
    BLP,    // Blorp
    BMP2,   // Bitmap version 2
    BMP3,   // Bitmap version 3
    BRF,    // BRF ASCII Braille
    CALS,   // CALS raster
    CAPTION,// Image caption
    CLIP,   // ImageMagick Clip path
    CMYK,   // raw cyan, magenta, yellow, black
    CMYKA,  // raw cyan, magenta, yellow, black, opacity
    CUR,    // Microsoft cursor
    CUT,    // Halo image
    DCM,    // DICOM image
    DCX,    // ZSoft PC Paintbrush
    DDS,    // DirectDraw Surface
    DIB,    // Device Independent Bitmap
    DJVU,   // DjVu
    DOT,    // Graphviz DOT
    DPR,    // Disney Pixar format
    EMF,    // Enhanced Metafile
    EPS2,   // PostScript Level II
    EPS3,   // PostScript Level III
    EPSI,   // Encapsulated PostScript Interchange
    FAX,    // Group 3 FAX
    FONT,   // Font file
    FPX,    // FlashPix
    FRACTAL,// Fractal image
    FTS,    // FITS
    G,      // G (generic)
    K,      // K (generic)
    G3,     // Group 3 FAX
    G4,     // Group 4 CCITT
    GIF87,  // GIF 87
    GRADIENT,// Gradient
    GRAY,   // Grayscale
    GRAYA,  // Grayscale with alpha
    GRBO,   // Gremlin object
    H,      // H (internal)
    HALD,   // Identity Hald CLUT
    HCL,    // HCL
    HISTOGRAM,// Histogram
    HRZ,    // Slow Scan TV
    HTM,    // HTML
    HTML,   // HTML
    ICB,    // Truevision Targa
    ICC,    // ICC profile
    ICO,    // Windows icon
    ICER,   // ICER
    INFO,   // Formatted text info
    INLINE, // Base64 inline
    IPL,    // IPL
    ISOBRL, // ISO Braille
    ISOBRL6,// ISO Braille 6
    JBG,    // Joint Bi-level Image
    JNG,    // JPEG Network Graphics
    JNX,    // Garmin tile
    JPE,    // JPEG
    JPM,    // JPEG 2000 Part 6
    JPS,    // Stereo
    JPX,    // JPEG 2000 Part 14
    KVEC,   // Vector
    LABEL,  // Label
    LINUX,  // Linux
    MAC,    // MacPaint
    MAP,    // Map
    MAT,    // MATLAB
    MATTE,  // Matte
    MNG,    // Multiple-image Network Graphics
    MONO,   // Bi-level
    MPO,    // MPO image
    MSL,    // MSL
    MTV,    // MTV raytracer
    MVG,    // Magick Vector Graphics
    NULLIMG,   // Null image
    ORA,    // OpenRaster
    OTB,    // IRIS bitmap
    PAL,    // Atari PAL
    PALM,   // Palm Pixmap
    PBM,    // Portable bitmap
    PCDS,   // Photo CD Smoothing
    PCX,    // PC Paintbrush
    PDB,    // Palm Database
    PFA,    // PostScript font A
    PFB,    // PostScript font B
    PGM,    // Portable graymap
    PICON,  // Person of interest
    PIX,    // Alias/Wavefront
    PJPEG,  // Progressive JPEG
    PLASMA, // Plasma
    PNG8,   // PNG 8-bit
    PNG24,  // PNG 24-bit
    PNG32,  // PNG 32-bit
    PNG48,  // PNG 48-bit
    PNG64,  // PNG 64-bit
    PNM,    // Portable anymap
    POCKETMOD,// Pocketmod
    PS,     // PostScript
    PS2,    // PostScript Level II
    PS3,    // PostScript Level III
    PTIF,   // Pyramid TIFF
    PWP,    // Seattle Film Works
    RAS,    // SUN raster
    SUN,    // Sun raster
    RGB,    // RGB
    RGB565, // RGB 565
    RGBA,   // RGBA
    RGBA555,// RGBA 555
    RGF,    // LEGO Mindstorms RGF
    RLA,    // Wavefront
    RLE,    // Utah RLE
    RPM,    // RedHat Package Manager
    RSLE,   // Run-length encoded
    SFW,    // Seattle Film Works
    SI,     // Silicon Imaging
    SHTML,  // Server-side image map
    STREAM, // LightWave stream
    SVGZ,   // Compressed SVG
    TEXT,   // Text
    TILE,   // Tile
    TIM,    // PSX TIM
    TM2,    // PS2 TIM2
    TTC,    // TrueType collection
    TTF,    // TrueType font
    TXT,    // Text
    UBRL,   // Unicode Braille
    UBRL6,  // Unicode Braille 6
    UIL,    // X-Motif UIL
    UYVY,   // 16bit YUV
    VDA,    // Truevision Targa
    VIFF,   // Khoros VIFF
    VIPS,   // VIPS image
    WBMP,   // Wireless Bitmap
    WDP,    // Windows Media Photo
    WING,   // Wingdings
    WPG,    // WordPerfect
    X,      // X Image
    XBM,    // X11 bitmap
    XC,     // Constant image
    XCF,    // GIMP XCF
    XPM,    // X11 pixmap
    XV,     // Khoros XV
    XWD,    // X Window dump
    YUV,    // YUV
    YUVA    // YUV with alpha
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

    // Extract embedded thumbnail (fast, no full demosaic)
    // Returns true if thumbnail was extracted, false if not available
    // If thumbnail is not available, output is unmodified
    virtual bool getThumbnail(const uint8_t* buffer, size_t size, int maxSize, ImageData& output) = 0;

    // Stream decode - yields tiles for memory-efficient processing
    // Returns number of tiles generated, 0 if streaming not supported
    // Tiles are written to outputTiles vector
    virtual size_t stream(const uint8_t* buffer, size_t size, int tileSize, std::vector<ImageData>& outputTiles) = 0;

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
    bool getThumbnail(const uint8_t* buffer, size_t size, int maxSize, ImageData& output) override;
    size_t stream(const uint8_t* buffer, size_t size, int tileSize, std::vector<ImageData>& outputTiles) override;
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
    bool getThumbnail(const uint8_t* buffer, size_t size, int maxSize, ImageData& output) override;
    size_t stream(const uint8_t* buffer, size_t size, int tileSize, std::vector<ImageData>& outputTiles) override;
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
    bool getThumbnail(const uint8_t* buffer, size_t size, int maxSize, ImageData& output) override;
    size_t stream(const uint8_t* buffer, size_t size, int tileSize, std::vector<ImageData>& outputTiles) override;
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
