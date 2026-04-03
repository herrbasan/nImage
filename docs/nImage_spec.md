# nImage Architecture Specification

**Last Updated**: 2026-04-03

## Overview

nImage is a native Node.js module (NAPI) for high-performance image processing. It provides:
- **Decode**: RAW (CR2, NEF, ARW, ORF, RAF, DNG), HEIC/HEIF, and standard formats
- **Transform**: Via Sharp (resize, crop, rotate, composite)
- **Encode**: JPEG, PNG, WebP, AVIF output

## Design Philosophy

nImage is a **codec-first** module. It handles the native library complexity for exotic formats, then delegates transformation work to Sharp which is already optimized for common operations.

```
┌─────────────────────────────────────────────────────────────────┐
│                         nImage Module                            │
│                                                                  │
│  Input              Decode           Transform        Encode      │
│  ──────            ──────           ─────────        ─────       │
│  RAW ─────────────► RGB/RGBA ──────► Sharp ◄──────── PNG        │
│  HEIC ────────────►              ──►        ───────► JPEG       │
│  HEIF                                              ──► WebP      │
│  JPEG ◄──────────────────────────────────────────────► AVIF      │
│  PNG                                                 (via Sharp) │
│  WebP                                                (via Sharp) │
│  TIFF                                               (via Sharp) │
└─────────────────────────────────────────────────────────────────┘

Legend: ──► native    ──► Sharp (libvips)
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
| JPEG | libjpeg | ⬜ Future | RGB |
| PNG | libpng | ⬜ Future | RGB/RGBA |
| WebP | libwebp | ⬜ Future | RGB/RGBA |
| TIFF | libtiff | ⬜ Future | RGB/RGBA |

### Output Formats (Encoding)

| Format | Library | Status | Notes |
|--------|---------|--------|-------|
| JPEG | libjpeg | 🔲 TODO | Baseline encoding |
| PNG | libpng | 🔲 TODO | With compression levels |
| WebP | libwebp | 🔲 TODO | Lossy/Lossless |
| AVIF | libaom | 🔲 TODO | Via Sharp initially |

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

```javascript
const nImage = require('nimage');

// Detection (always available - pure JS)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decoding
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

### Usage Example

```javascript
const nImage = require('nimage');
const sharp = require('sharp');

// Process a RAW file
const rawBuffer = fs.readFileSync('photo.cr2');
const imageData = nImage.decode(rawBuffer);

// Transform with Sharp
const transformed = await sharp(Buffer.from(imageData.data), {
    raw: {
        width: imageData.width,
        height: imageData.height,
        channels: imageData.channels
    }
})
.resize(1024, 1024, { fit: 'inside' })
.jpeg({ quality: 85 })
.toBuffer();

// Or use nImage encoder directly
const jpegBuffer = nImage.encode(
    imageData.data,
    imageData.width,
    imageData.height,
    imageData.channels,
    'jpeg',
    { quality: 85 }
);
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
