# nImage Architecture Specification

**Last Updated**: 2026-04-03

## Overview

nImage is a native Node.js module (NAPI) for high-performance image decoding. It provides in-process access to native image libraries without CLI spawn overhead.

## Module Structure

```
nImage/
├── src/
│   ├── decoder.h           # Base decoder class and structures
│   ├── decoder.cpp         # Decoder implementations (LibRaw, LibHeif)
│   └── binding.cpp         # NAPI bindings
├── lib/
│   └── index.js           # JavaScript entry point
├── dist/                  # Pre-compiled binaries (tracked in git)
│   ├── nimage.node       # Native module
│   └── *.dll             # Runtime DLLs
├── deps/                  # External dependencies (gitignored)
│   ├── include/          # Header files
│   ├── lib/              # Import libraries
│   └── bin/              # DLLs
├── scripts/
│   ├── setup.ps1         # Full setup script
│   └── build.js          # Custom build script (direct g++)
├── test/
│   ├── index.test.js     # Unit tests
│   ├── benchmark.js      # Performance benchmarks
│   └── convert-to-jpeg.js # Conversion tool
└── package.json
```

## Class Hierarchy

### ImageDecoder (Base Class)

Abstract base class for all image decoders.

```cpp
class ImageDecoder {
    virtual bool decode(const uint8_t* buffer, size_t size, ImageData& output) = 0;
    virtual bool getMetadata(const uint8_t* buffer, size_t size, ImageMetadata& metadata) = 0;
    virtual bool supportsFormat(ImageFormat format) const = 0;
    virtual const char* formatName() const = 0;
};
```

### LibRawDecoder ✅ IMPLEMENTED

Handles RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)

Uses libraw 0.22.0 library for:
- RAW file parsing via open_buffer()
- Unpacking via unpack()
- Demosaicing via dcraw_process()
- Pixel extraction via dcraw_make_mem_image()
- Metadata extraction (camera info, EXIF)

### LibHeifDecoder ⏳ NOT IMPLEMENTED

Handles HEIC/HEIF formats. Stub exists but decode() returns error.

Uses libheif library for:
- HEIF/HEIC file parsing
- Image decoding
- Thumbnail extraction
- Metadata extraction

## Data Structures

### ImageData

Output structure returned from decode operations.

```cpp
struct ImageData {
    std::vector<uint8_t> data;      // Pixel data (RGB)
    int width, height;              // Dimensions
    int bitsPerChannel;             // Bit depth (8)
    int channels;                   // 3=RGB
    std::string colorSpace;         // Color space name
    std::string format;              // Original format
    std::string cameraMake, cameraModel;
    double exposureTime, fNumber;
    int isoSpeed;
    double focalLength;
    std::string dateTime;
    int orientation;
    bool hasAlpha;
};
```

### ImageMetadata

Lightweight metadata without full decode.

```cpp
struct ImageMetadata {
    std::string format;
    int width, height;
    int rawWidth, rawHeight;
    std::string cameraMake, cameraModel;
    int isoSpeed;
    double exposureTime, fNumber, focalLength;
    std::string colorSpace;
    bool hasAlpha;
    int bitsPerSample;
    int orientation;
    size_t fileSize;
};
```

## NAPI Bindings

### ImageDecoderWrapper

NAPI ObjectWrap class that exposes ImageDecoder to JavaScript.

```cpp
class ImageDecoderWrapper : public Napi::ObjectWrap<ImageDecoderWrapper> {
    static Napi::Object Init(Napi::Env env, Napi::Object exports);

    Napi::Value Decode(const Napi::CallbackInfo& info);
    Napi::Value GetMetadata(const Napi::CallbackInfo& info);
    Napi::Value GetError(const Napi::CallbackInfo& info);
};
```

### Exported Functions

```javascript
exports.detectFormat(buffer)     // Detect format from buffer
exports.getSupportedFormats()    // Get list of supported formats
exports.decode(buffer, [hint])  // Decode image directly
exports.ImageDecoder            // Class for batch decoding
```

## JavaScript API Layer

`lib/index.js` provides:

1. **Native module loading** - Tries `dist/` then `build/Release/`
2. **Fallback API** - Graceful degradation when native module unavailable
3. **Format detection** - Pure JS implementation for common formats
4. **Error handling** - Helpful messages for setup issues

## Build Process

### Windows Build (Custom g++ Approach)

The build uses a custom script that bypasses node-gyp on Windows because node-gyp defaults to MSVC even with `--compiler=mingw`.

1. `npm run build` - Invokes `scripts/build.js`
2. `build.js`:
   - Sets up MSYS2 environment (MSYSTEM=MINGW64)
   - Finds Node.js headers from node-gyp cache
   - Creates NAPI import library from node.lib using dlltool
   - Invokes g++ with proper include/library paths
   - Copies artefact and DLLs to dist/

### Key Build Files

- `binding.gyp` - Original node-gyp config (used for reference)
- `scripts/build.js` - Custom build script (active)
- `scripts/setup.ps1` - Full setup including deps

## Dependencies

### libraw (✅ Working)

- **Version**: 0.22.0 (via MSYS2)
- **Package**: mingw-w64-x86_64-libraw
- **Website**: https://www.libraw.org/
- **License**: LGPL/GPL
- **Purpose**: RAW image decoding

### libheif (⏳ Not Implemented)

- **Version**: 1.21.2 (via MSYS2)
- **Package**: mingw-w64-x86_64-libheif
- **Website**: https://github.com/strukturag/libheif
- **License**: LGPL
- **Purpose**: HEIC/HEIF/AVIF decoding
- **Dependencies**: libde265 (HEVC), aom (AV1), x265, libpng, libjpeg

## LibRaw 0.22 API Notes

The code was updated for LibRaw 0.22 API compatibility:

| Old API | New API |
|---------|---------|
| `raw->close()` | Removed (use only `recycle()`) |
| `raw->unpack2()` | Removed (not in 0.22) |
| `raw->dcraw_free_mem_image()` | `LibRaw::dcraw_clear_mem()` (static) |
| `sizes.output_width/height` | `sizes.width/height` |
| `params.shot_select` | `rawparams.shot_select` (unsigned) |
| `lens.lensmodel` | `lens.makernotes.Lens` |
| `other.datetime` | `other.timestamp` (time_t) + strftime |

## Performance Characteristics

1. **Format detection**: O(1) - only reads first 12 bytes for magic detection
2. **Memory allocation**: ImageData uses std::vector for dynamic pixel buffers
3. **Buffer copies**: NAPI Buffer::Copy used to avoid memory leaks
4. **Metadata extraction**: Can be done without full demosaicing
5. **Decode time**: Proportional to image size and camera model

### Benchmark Results

```
Format Detection: ~0.5-0.6 µs per call (1.7-2M ops/sec)
  - Same speed regardless of buffer size (12 bytes to 6MB)

RAW Decode:
  - Olympus ORF (9MB): 3720x2800 → ~400ms
  - Canon CR2 (22MB): 5208x3476 → ~520ms
```

## Future Enhancements

- [ ] Full libheif integration with HEIC/HEIF/AVIF support
- [ ] JPEG encoding (output to JPEG files)
- [ ] ICC color profile application
- [ ] Tile-based decoding for large images
- [ ] Streaming decode support
- [ ] Write support (encode to RAW/HEIC)
