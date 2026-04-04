# nImage Architecture Specification

**Last Updated**: 2026-04-04

**Version**: 2.2.0 - Phase 8: Error propagation & Thumbnail extraction

## Overview

nImage is a native Node.js module (NAPI) for high-performance image processing. It provides:
- **Decode**: RAW (CR2, NEF, ARW, ORF, RAF, DNG), HEIC/HEIF, and 150+ formats via ImageMagick
- **Transform**: Via Sharp (resize, crop, rotate, composite)
- **Encode**: JPEG, PNG, WebP, AVIF output

## Design Philosophy

nImage is a **Sharp-compatible API wrapper** that adds native codec support for formats Sharp cannot handle directly. The pipeline is:

```
User calls nImage API (Sharp-compatible)
         │
         ▼
┌─────────────────────────────────────────────────────┐
│                    nImage                            │
│                                                      │
│  1. Detect input format                             │
│  2. If RAW → LibRawDecoder to RGB                   │
│  3. If HEIC/AVIF → LibHeifDecoder to RGB           │
│  4. If other format → MagickDecoder (ImageMagick)   │
│  5. Pass RGB buffer to Sharp                         │
│  6. Sharp handles all transforms/encode              │
│  7. Return result to user                           │
└─────────────────────────────────────────────────────┘
```

**ImageMagick is the catch-all fallback** for any format not handled by LibRaw or LibHeif. This enables support for 150+ additional formats including documents (PDF, SVG, AI, DOCX, XLSX, PPTX), scientific formats (EXR, HDR, DPX, FITS), and video stills (AVI, MOV, MP4, MKV).

### Why This Architecture?

- **Sharp is already excellent** at transforms and common format encoding (JPEG, PNG, WebP, AVIF)
- **Native codecs are complex**: RAW demosaicing and HEIC decoding require specialized libraries (libraw, libheif)
- **ImageMagick fills the gaps**: For formats Sharp can't handle (PDF, SVG, documents, scientific formats), MagickCore provides comprehensive support
- **Single API surface**: Users interact with nImage exactly as they would with Sharp, but gain support for 212 formats
- **Maintainability**: Sharp's libvips-based pipeline is well-tested; we only maintain the exotic codec integration

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                              nImage Module                                     │
│                                                                               │
│  Input              Specialized          Sharp处理              Output           │
│  ──────             ───────────         ─────────            ─────            │
│  RAW ─────────────► [LibRawDecoder]──► [Transform]────────► JPEG            │
│  HEIC/AVIF ───────► [LibHeifDecoder]──► [Resize]──────────► PNG            │
│  JPEG/PNG/WebP ───► [Sharp]──────────► [Crop]────────────► WebP            │
│  PDF/SVG/DOCX ────► [MagickDecoder]──► [Rotate]─────────► AVIF            │
│  EXR/HDR/CIN ─────► (ImageMagick)────► [Any Op]──────────► (any)           │
│  150+ formats ────► (fallback)────────► ──────────────────► ────────────────►│
└───────────────────────────────────────────────────────────────────────────────┘

MagickDecoder handles all formats not covered by LibRaw or LibHeif.
User-facing API is Sharp-compatible.
```

## Module Structure

```
nImage/
├── src/
│   ├── decoder.h           # Base decoder class, ImageFormat enum (212 formats)
│   ├── decoder.cpp         # LibRawDecoder, LibHeifDecoder, MagickDecoder
│   ├── encoder.h           # Base encoder class and structures
│   ├── encoder.cpp         # Encoder implementations (JPEG, PNG, WebP)
│   └── binding.cpp         # NAPI bindings
├── lib/
│   └── index.js           # JavaScript entry point + Sharp wrapper
├── dist/                  # Pre-compiled binaries (tracked in git)
│   ├── nimage.node       # Native module
│   └── *.dll             # 116 runtime DLLs
├── deps/                  # External dependencies (gitignored)
├── scripts/
│   ├── setup.ps1         # Full setup script
│   └── build.js          # Custom build script (direct g++)
├── test/
│   ├── index.test.js     # Unit tests
│   ├── benchmark.js       # Performance benchmarks
│   └── assets/           # Test images
├── docs/                 # Documentation
└── package.json
```

## Supported Formats (212 total)

### RAW Formats (LibRaw) - 22 formats

| Format | Library | Status |
|--------|---------|--------|
| Canon CR2/CRW | libraw | ✅ Working |
| Nikon NEF/NRW | libraw | ✅ Working |
| Sony ARW/SR2 | libraw | ✅ Working |
| Olympus ORF | libraw | ✅ Working |
| Fujifilm RAF | libraw | ✅ Working |
| Panasonic RW2 | libraw | ✅ Working |
| Adobe DNG | libraw | ✅ Working |
| Pentax PEF/PEFR | libraw | ✅ Working |
| Samsung SRW | libraw | ✅ Working |
| Leica RWL/MRAW | libraw | ✅ Working |
| Minolta MRW | libraw | ✅ Working |
| Epson ERF | libraw | ✅ Working |
| Hasselblad 3FR | libraw | ✅ Working |
| Kodak K25/KDC | libraw | ✅ Working |
| Mamiya MEF | libraw | ✅ Working |
| Leaf MOS | libraw | ✅ Working |
| Canon RRF | libraw | ✅ Working |
| Rawzor RWZ | libraw | ✅ Working |
| Gremlin GRB | libraw | ✅ Working |

### HEIC/AVIF (LibHeif) - 3 formats

| Format | Library | Status |
|--------|---------|--------|
| HEIC | libheif | ✅ Working |
| HEIF | libheif | ✅ Working |
| AVIF | libheif | ✅ Working |

### Standard Formats (Sharp) - 9 formats

| Format | Library | Status |
|--------|---------|--------|
| JPEG | Sharp | ✅ Working |
| PNG | Sharp | ✅ Working |
| WebP | Sharp | ✅ Working |
| TIFF | Sharp | ✅ Working |
| GIF | Sharp | ✅ Working |
| BMP | Sharp | ✅ Working |
| JXL (JPEG XL) | Sharp | ✅ Working |
| JP2 (JPEG 2000) | Sharp | ✅ Working |

### Document Formats (ImageMagick) - 13 formats

| Format | Library | Status |
|--------|---------|--------|
| PDF | MagickCore | ✅ Working |
| SVG | MagickCore | ✅ Working |
| AI | MagickCore | ✅ Working |
| DOC/DOCX | MagickCore | ✅ Working |
| XLS/XLSX | MagickCore | ✅ Working |
| PPT/PPTX | MagickCore | ✅ Working |
| EPS | MagickCore | ✅ Working |
| XPS | MagickCore | ✅ Working |
| PSD/PSB | MagickCore | ✅ Working |

### Scientific Formats (ImageMagick) - 20 formats

| Format | Library | Status |
|--------|---------|--------|
| EXR | MagickCore | ✅ Working |
| HDR | MagickCore | ✅ Working |
| DPX | MagickCore | ✅ Working |
| FITS | MagickCore | ✅ Working |
| FLIF | MagickCore | ✅ Working |
| CIN | MagickCore | ✅ Working |
| JP2/J2K | MagickCore | ✅ Working |
| MIFF | MagickCore | ✅ Working |
| MPC | MagickCore | ✅ Working |
| PCD | MagickCore | ✅ Working |
| PFM | MagickCore | ✅ Working |
| PICT | MagickCore | ✅ Working |
| PPM | MagickCore | ✅ Working |
| PSP | MagickCore | ✅ Working |
| SGI | MagickCore | ✅ Working |
| TGA | MagickCore | ✅ Working |
| VTF | MagickCore | ✅ Working |
| BigTIFF | MagickCore | ✅ Working |

### Video Stills (ImageMagick) - 12 formats

| Format | Library | Status |
|--------|---------|--------|
| AVI | MagickCore | ✅ Working |
| MOV | MagickCore | ✅ Working |
| MP4/M4V | MagickCore | ✅ Working |
| MPG/MPEG | MagickCore | ✅ Working |
| WMV | MagickCore | ✅ Working |
| FLV | MagickCore | ✅ Working |
| MKV | MagickCore | ✅ Working |
| MNG | MagickCore | ✅ Working |
| JNG | MagickCore | ✅ Working |
| MPO | MagickCore | ✅ Working |
| DCM | MagickCore | ✅ Working |

### Other Formats (ImageMagick) - 150+ formats

AAI, ART, BLP, BMP2, BMP3, CUR, DIB, DDS, DJVU, EMF, GRAY, GRAYA, ICB, ICC, ICO, MAT, MATTE, MONO, PAL, PALM, PBM, PCDS, PDB, PCX, DCX, PFA, PFB, PGM, PICON, PIX, PJPEG, PLASMA, PNG8, PNG24, PNG32, PNM, RAS, RGB, RGBA, RGB565, RGBA555, RLE, SFW, SUN, TILE, WBMP, WMF, WPG, XBM, XPM, XWD, and many more.

### Output Formats (Encoding)

All output encoding is handled by Sharp's libvips pipeline.

| Format | Library | Status | Notes |
|--------|---------|--------|-------|
| JPEG | Sharp | ✅ Working | Quality 1-100 |
| PNG | Sharp | ✅ Working | Compression levels |
| WebP | Sharp | ✅ Working | Lossy/Lossless |
| AVIF | Sharp | ✅ Working | Quality 1-100 |
| TIFF | Sharp | ✅ Working | Compression |

## Data Structures

### ImageData (Decoder Output)

```cpp
struct ImageData {
    std::vector<uint8_t> data;      // Pixel data (RGB or RGBA)
    int width = 0;                  // Image width
    int height = 0;                 // Image height
    int bitsPerChannel = 8;         // Bit depth per channel
    int channels = 3;              // 3=RGB, 4=RGBA
    std::string colorSpace;        // "sRGB", "Adobe RGB", etc.
    std::vector<uint8_t> iccProfile; // ICC profile data
    std::string format;            // Original format (e.g., "cr2", "heic")
    bool hasAlpha = false;
    // Camera metadata
    std::string cameraMake;
    std::string cameraModel;
    // Capture settings
    std::string dateTime;
    double exposureTime = 0.0;
    double fNumber = 0.0;
    int isoSpeed = 0;
    double focalLength = 0.0;
    // RAW dimensions
    int rawWidth = 0;
    int rawHeight = 0;
    // Orientation (EXIF 1-8)
    int orientation = 1;
};
```

### ImageMetadata (Lightweight)

```cpp
struct ImageMetadata {
    std::string format;
    int width = 0;
    int height = 0;
    int rawWidth = 0;
    int rawHeight = 0;
    std::string cameraMake;
    std::string cameraModel;
    int isoSpeed = 0;
    double exposureTime = 0.0;
    double fNumber = 0.0;
    double focalLength = 0.0;
    std::string colorSpace;
    bool hasAlpha = false;
    int bitsPerSample = 0;
    int orientation = 1;
    size_t fileSize = 0;
    std::string lensModel;
};
```

### EncoderOptions

```cpp
struct EncoderOptions {
    int quality = 85;              // 1-100 for lossy formats
    bool stripExif = true;         // Remove metadata
    int compressionLevel = 6;      // PNG: 0-9, TIFF: 1-8
    bool lossless = false;         // WebP lossless mode
};
```

## NAPI API

### JavaScript API

nImage presents a **Sharp-compatible API**. Internally it uses Sharp for transforms and encoding, but adds RAW/HEIC/ImageMagick support by decoding those formats first.

```javascript
const nImage = require('nimage');

// Sharp-compatible pipeline (nImage handles RAW/HEIC/ImageMagick formats internally)
const result = await nImage('photo.cr2')  // RAW file
    .resize(1024, 1024, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();

// PDF to image
const pdfThumb = await nImage('document.pdf')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Same API for HEIC
const result = await nImage('image.heic')
    .rotate(90)
    .webp({ quality: 80 })
    .toBuffer();

// Standard formats work directly through Sharp
const result = await nImage('photo.jpg')
    .resize(800, 600)
    .toBuffer();

// Detection (always available - pure JS)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Get all supported formats
nImage.getSupportedFormats() → ['cr2', 'nef', 'heic', 'pdf', ...] // 212 formats

// Low-level decode (when you need raw pixel access)
nImage.decode(buffer, [formatHint]) → ImageData
decoder.decode(buffer) → ImageData

// Encoding
nImage.encode(rgbBuffer, width, height, channels, format, options) → Buffer
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer

// ImageDecoder class
const decoder = new nImage.ImageDecoder([format])
decoder.decode(buffer) → ImageData
decoder.getMetadata(buffer) → ImageMetadata

// ImageEncoder class
const encoder = new nImage.ImageEncoder('jpeg')
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer
```

### Usage Examples

```javascript
const nImage = require('nimage');

// Pipeline with RAW input - nImage handles decoding transparently
const thumb = await nImage('photo.cr2')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 80 })
    .toBuffer();

// Pipeline with PDF input - ImageMagick rasterizes to image
const pdfThumb = await nImage('document.pdf')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Pipeline with HEIC input
const optimized = await nImage('image.heic')
    .resize(1920, 1080, { fit: 'inside' })
    .webp({ quality: 85 })
    .toBuffer();

// Low-level: Decode any format to get raw RGB for custom processing
const imageData = nImage.decode(fs.readFileSync('document.pdf'));
console.log(imageData.width, imageData.height);
console.log(imageData.format);  // 'pdf'

// Then pass to Sharp manually if needed
const sharp = require('sharp');
const transformed = await sharp(Buffer.from(imageData.data), {
    raw: {
        width: imageData.width,
        height: imageData.height,
        channels: imageData.channels  // 3=RGB, 4=RGBA
    }
})
.resize(1024, 1024)
.jpeg()
.toBuffer();
```

## Pipeline Integration

### Decoder Priority Chain

```
Format Detection
       │
       ▼
┌─────────────────────────────────────┐
│ Try LibRaw (RAW formats)             │──▶ Success ──▶ RGB → Sharp
├─────────────────────────────────────┤
│ Try LibHeif (HEIC/HEIF/AVIF)       │──▶ Success ──▶ RGB → Sharp
├─────────────────────────────────────┤
│ Try MagickDecoder (ALL other)       │──▶ Success ──▶ RGB → Sharp
└─────────────────────────────────────┘
                 │
                 ▼
            Error / Unsupported
```

### MediaService Pipeline

```
HTTP Request
     │
     ▼
┌─────────────────┐
│ Format Detection │ (nImage.detectFormat)
└────────┬────────┘
         │
    ┌────┴────┐
    │ Format?  │
    └────┬────┘
         │
    ┌────┼────┬────────┐
    │    │    │        │
    ▼    ▼    ▼        ▼
  RAW  HEIC  Std   Other
    │    │    │      │
    │    │    │      └──► [MagickDecoder] ─► RGB
    │    │    └──► [Sharp] ────────────────► RGB
    │    └──► [LibHeifDecoder] ───────────► RGB
    └──► [LibRawDecoder] ─────────────────► RGB
         │
         ▼
┌─────────────────┐
│ Sharp Transform │ (resize, crop, rotate)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Sharp Encode    │ (jpeg, png, webp, avif)
└────────┬────────┘
         │
         ▼
    Output Buffer
```

## Performance Targets

| Operation | Target | Sharp Comparison |
|-----------|--------|-----------------|
| Format detection | < 1 µs | Equivalent |
| RAW decode (20MP) | < 600ms | FFmpeg: ~800ms |
| HEIC decode (12MP) | < 150ms | FFmpeg: ~400ms |
| ImageMagick decode | Varies | N/A |
| JPEG encode | < 100ms | Equivalent |
| PNG encode | < 150ms | Equivalent |

## Dependencies

### Build Dependencies

| Library | Package | Purpose |
|---------|---------|---------|
| libraw 0.22 | mingw-w64-x86_64-libraw | RAW decoding |
| libheif 1.21 | mingw-w64-x86_64-libheif | HEIC/HEIF decoding |
| ImageMagick 7 | mingw-w64-x86_64-imagemagick | 150+ format fallback |
| libjpeg | mingw-w64-x86_64-libjpeg | JPEG encode/decode |
| libpng | mingw-w64-x86_64-libpng | PNG encode/decode |
| libwebp | mingw-w64-x86_64-libwebp | WebP encode/decode |
| libtiff | mingw-w64-x86_64-libtiff | TIFF encode/decode |
| libaom | mingw-w64-x86_64-aom | AV1/AVIF encode |

### Runtime Dependencies (in dist/)

```
dist/
├── nimage.node              # Main module
├── libraw-24.dll           # RAW decoding
├── libheif.dll             # HEIC/HEIF decoding
├── libMagickCore-7.Q16HDRI-10.dll  # ImageMagick core
├── libjpeg-8.dll           # JPEG codec
├── libpng16-16.dll         # PNG codec
├── libwebp-7.dll           # WebP codec
├── libtiff-6.dll           # TIFF codec
├── libaom.dll              # AV1 codec
├── libde265-0.dll          # HEVC decoder
├── libx265.dll             # H.265 codec
├── libx264.dll             # H.264 codec
└── ... (100+ other DLLs)
```

## Build System

### Windows (MSYS2 + MinGW g++)

```powershell
# Full setup
.\scripts\setup.ps1

# Build
npm run build
```

### Linux

```bash
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev libtiff-dev libmagick-dev
npm run build
```

## Error Handling

### Error Propagation (v2.2.0)

Pipeline errors are enhanced with context:

```javascript
try {
  await nImage('photo.cr2')
    .resize(999999)  // invalid size
    .jpeg()
    .toBuffer();
} catch (err) {
  console.log(err.message);
  // "nImage pipeline failed (resize → jpeg): maximum image dimension is 100000"

  console.log(err.originalError);
  // Original Sharp/libvips error

  console.log(err.pipeline);
  // { input, operations, outputFormat, formatDetected }
}
```

### Error Codes

| Error Code | Cause | Recovery |
|------------|-------|----------|
| `UNSUPPORTED_FORMAT` | Unknown format | Use `detectFormat` to check |
| `DECODE_FAILED` | Corrupt file / missing codec | Check error message |
| `ENCODE_FAILED` | Invalid parameters | Check format/quality values |
| `OUT_OF_MEMORY` | Image too large | Use tile-based processing |
| `MODULE_NOT_LOADED` | Build missing | Run `npm run setup` |

## Future Enhancements

- [ ] Multi-page PDF support (render any page to image)
- [ ] LittleCMS integration for ICC color management
- [ ] Tile-based decoding for memory efficiency
- [ ] Streaming decode/encode for large files
- [x] Thumbnail extraction without full decode
- [ ] EXIF manipulation
- [ ] Multi-page TIFF support

## Thumbnail Extraction

**Fast thumbnail extraction** without full image decode:

```
Input Buffer
     │
     ▼
Format Detection (pure JS magic bytes)
     │
     ▼
┌────────────────────────────────────────────────────────────┐
│ RAW/HEIC?           │ Standard Format?  │ Other?          │
│ (CR2, NEF, ARW...)  │ (JPEG, PNG...)    │ (PDF, PSD...)   │
└────────────────────────────────────────────────────────────┘
        │                      │                  │
        ▼                      ▼                  ▼
Native thumbnail        Sharp resize()     MagickDecoder
extraction             (libvips fast)     (slow fallback)
(~5-10ms)             (~10-50ms)         (full decode)
```

**Thumbnail Support:**

| Format Type | Thumbnail Method | Speed |
|-------------|-----------------|-------|
| RAW (CR2, NEF, ARW, ORF, RAF, DNG) | Native `unpack_thumb()` + `dcraw_make_mem_thumb()` | ✅ Fast |
| HEIC/HEIF/AVIF | Native `heif_image_handle_get_thumbnail()` | ✅ Fast |
| JPEG, PNG, WebP, GIF, TIFF, AVIF, BMP | Sharp `resize()` | ✅ Fast |
| PDF, PSD, EXR, HDR, SVG, etc. | MagickDecoder resize | ⚠️ Slow |

**API:**

```javascript
// Standalone function
const thumb = await nImage.thumbnail(buffer, { size: 256 });

// Pipeline method
const thumb = await nImage('photo.cr2').thumbnail({ size: 256 });

// Returns ImageData with width, height, channels, data, format
```

**Note:** All encoding is handled by Sharp (JPEG, PNG, WebP, AVIF, TIFF). nImage does not need separate encoders.
