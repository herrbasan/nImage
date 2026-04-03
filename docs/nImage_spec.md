# nImage Architecture Specification

**Last Updated**: 2026-04-03

**Version**: 2.0.0 - Sharp integration complete

## Overview

nImage is a native Node.js module (NAPI) for high-performance image processing. It provides:
- **Decode**: RAW (CR2, NEF, ARW, ORF, RAF, DNG), HEIC/HEIF, and standard formats
- **Transform**: Via Sharp (resize, crop, rotate, composite)
- **Encode**: JPEG, PNG, WebP, AVIF output

## Design Philosophy

nImage is a **Sharp-compatible API wrapper** that adds native codec support for formats Sharp cannot handle directly. The pipeline is:

```
User calls nImage API (Sharp-compatible)
         │
         ▼
┌─────────────────────────────────────────┐
│              nImage                       │
│                                          │
│  1. Detect input format                   │
│  2. If RAW/HEIC → decode to RGB via NAPI  │
│  3. Pass RGB buffer to Sharp              │
│  4. Sharp handles all transforms/encode   │
│  5. Return result to user                 │
└─────────────────────────────────────────┘
```

**Sharp is the main show** for transformations and encoding. nImage exists to bridge the gap for RAW and HEIC formats that Sharp cannot decode natively.

### Why This Architecture?

- **Sharp is already excellent** at transforms and common format encoding (JPEG, PNG, WebP, AVIF)
- **Native codecs are complex**: RAW demosaicing and HEIC decoding require specialized libraries (libraw, libheif)
- **Single API surface**: Users interact with nImage exactly as they would with Sharp, but gain RAW/HEIC support
- **Maintainability**: Sharp's libvips-based pipeline is well-tested; we only maintain the exotic codec integration

```
┌─────────────────────────────────────────────────────────────────┐
│                         nImage Module                            │
│                                                                  │
│  Input              nImage处理          Sharp处理         Output   │
│  ──────            ───────           ─────────        ─────     │
│  RAW ─────────────► [Decode] ────────► [Transform] ──► JPEG     │
│  HEIC ───────────►              ────► [Resize]  ────► PNG      │
│  HEIF                             ───► [Crop]   ────► WebP     │
│  JPEG ────────────────────────────────────────────► AVIF       │
│  PNG  ────────────────────────────────────────────► (any)       │
│  WebP ────────────────────────────────────────────► (any)      │
│  TIFF ────────────────────────────────────────────► (any)      │
└─────────────────────────────────────────────────────────────────┘

nImage处理: libraw/libheif decoding to RGB/RGBA
Sharp处理: All transformations and format encoding

User-facing API is Sharp-compatible. RAW/HEIC are transparently decoded
by nImage's native codecs, then passed to Sharp for the pipeline.
```

## Module Structure

```
nImage/
├── src/
│   ├── decoder.h           # Base decoder class and structures
│   ├── decoder.cpp         # Decoder implementations (LibRaw, LibHeif)
│   ├── encoder.h           # Base encoder class and structures
│   ├── encoder.cpp         # Encoder implementations (JPEG, PNG, WebP)
│   ├── avif_encoder.cpp    # AVIF encoder (via libaom)
│   └── binding.cpp         # NAPI bindings
├── lib/
│   └── index.js           # JavaScript entry point
├── dist/                  # Pre-compiled binaries (tracked in git)
│   ├── nimage.node       # Native module
│   └── *.dll             # Runtime DLLs
├── deps/                  # External dependencies (gitignored)
├── scripts/
│   ├── setup.ps1         # Full setup script
│   └── build.js          # Custom build script (direct g++)
├── test/
│   ├── index.test.js     # Unit tests
│   ├── benchmark.js      # Performance benchmarks
│   └── assets/          # Test images
├── docs/                 # Documentation
└── package.json
```

## Supported Formats

### Input Formats (Decoding)

| Format | Library | Status | Output |
|--------|---------|--------|--------|
| Canon CR2/CRW | libraw | ✅ Working | RGB |
| Nikon NEF | libraw | ✅ Working | RGB |
| Sony ARW | libraw | ✅ Working | RGB |
| Olympus ORF | libraw | ✅ Working | RGB |
| Fujifilm RAF | libraw | ✅ Working | RGB |
| Panasonic RW2 | libraw | ✅ Working | RGB |
| Adobe DNG | libraw | ✅ Working | RGB |
| Pentax PEF | libraw | ✅ Working | RGB |
| Samsung SRW | libraw | ✅ Working | RGB |
| Leica RWL | libraw | ✅ Working | RGB |
| HEIC | libheif | ✅ Working | RGB/RGBA |
| HEIF | libheif | ✅ Working | RGB/RGBA |
| AVIF | libheif+aom | ✅ Working | RGB/RGBA |
| JPEG | Sharp | ✅ Working | RGB |
| PNG | Sharp | ✅ Working | RGB/RGBA |
| WebP | Sharp | ✅ Working | RGB/RGBA |
| TIFF | Sharp | ✅ Working | RGB/RGBA |
| GIF | Sharp | ✅ Working | RGB/RGBA |

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

nImage presents a **Sharp-compatible API**. Internally it uses Sharp for transforms and encoding, but adds RAW/HEIC support by decoding those formats first.

```javascript
const nImage = require('nimage');

// Sharp-compatible pipeline (nImage handles RAW/HEIC internally)
const result = await nImage('photo.cr2')  // RAW file
    .resize(1024, 1024, { fit: 'inside' })
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

// Pipeline with HEIC input
const optimized = await nImage('image.heic')
    .resize(1920, 1080, { fit: 'inside' })
    .webp({ quality: 85 })
    .toBuffer();

// Low-level: Decode RAW to get raw RGB for custom processing
const imageData = nImage.decode(fs.readFileSync('photo.cr2'));
console.log(imageData.width, imageData.height);  // 6000, 4000

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
    │ RAW/HEIC?│
    └────┬────┘
    Yes  │  No
    │    │
    ▼    │
┌──────────────┐    ┌─────────────────┐
│ nImage.decode│    │ Sharp (direct)  │
└────┬─────────┘    └────────┬────────┘
     │                       │
     ▼                       ▼
┌─────────────┐        ┌─────────────┐
│ Raw RGB     │        │ Sharp handles│
│ Buffer      │        │ everything   │
└────┬────────┘        └──────┬──────┘
     │                        │
     ▼                        │
┌─────────────────┐            │
│ Sharp.transform │◄───────────┤
│ (resize, crop)  │            │
└────────┬────────┘            │
         │                    │
         ▼                    │
┌─────────────────┐            │
│ nImage.encode   │◄───────────┘
│ or Sharp.encode │
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
| JPEG encode | < 100ms | Equivalent |
| PNG encode | < 150ms | Equivalent |

## Dependencies

### Build Dependencies

| Library | Package | Purpose |
|---------|---------|---------|
| libraw 0.22 | mingw-w64-x86_64-libraw | RAW decoding |
| libheif 1.21 | mingw-w64-x86_64-libheif | HEIC/HEIF decoding |
| libjpeg | mingw-w64-x86_64-libjpeg | JPEG encode/decode |
| libpng | mingw-w64-x86_64-libpng | PNG encode/decode |
| libwebp | mingw-w64-x86_64-libwebp | WebP encode/decode |
| libtiff | mingw-w64-x86_64-libtiff | TIFF encode/decode |
| libaom | mingw-w64-x86_64-aom | AV1/AVIF encode |
| aom | (from libheif dep) | AVIF decode |

### Runtime Dependencies (in dist/)

```
dist/
├── nimage.node              # Main module
├── libraw-24.dll           # RAW decoding
├── libheif.dll             # HEIC/HEIF decoding
├── libjpeg-8.dll           # JPEG codec
├── libpng16-16.dll         # PNG codec
├── libwebp-7.dll           # WebP codec
├── libtiff-6.dll           # TIFF codec
├── libaom.dll              # AV1 codec
├── libsvtav1enc-4.dll      # AV1 encoder
├── libde265-0.dll          # HEVC decoder
├── lcms2-2.dll             # Color management
└── ... (50+ other DLLs)
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
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev libtiff-dev
npm run build
```

## Error Handling

| Error Code | Cause | Recovery |
|------------|-------|----------|
| `UNSUPPORTED_FORMAT` | Unknown format | Use `detectFormat` to check |
| `DECODE_FAILED` | Corrupt file / missing codec | Check error message |
| `ENCODE_FAILED` | Invalid parameters | Check format/quality values |
| `OUT_OF_MEMORY` | Image too large | Use tile-based processing |
| `MODULE_NOT_LOADED` | Build missing | Run `npm run setup` |

## Future Enhancements

- [ ] LittleCMS integration for ICC color management
- [ ] Tile-based decoding for memory efficiency
- [ ] Streaming decode/encode for large files
- [ ] Thumbnail extraction without full decode
- [ ] EXIF manipulation
- [ ] Multi-page TIFF support
- [ ] GIF encoding
