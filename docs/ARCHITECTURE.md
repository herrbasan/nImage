# nImage Architecture

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
├── deps/                  # External dependencies (gitignored)
│   ├── libraw/            # libraw headers and libs
│   ├── libheif/           # libheif headers and libs
│   ├── win/lib/           # Windows libraries
│   └── linux/lib/         # Linux libraries
├── scripts/
│   └── vendor.js          # Dependency download script
├── binding.gyp            # node-gyp build configuration
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

### LibRawDecoder

Handles RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)

Uses libraw library for:
- RAW file parsing
- Demosaicing
- Basic color processing
- Metadata extraction

### LibHeifDecoder

Handles HEIC/HEIF formats.

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
    std::vector<uint8_t> data;      // Pixel data
    int width, height;              // Dimensions
    int bitsPerChannel;             // Bit depth
    int channels;                   // 3=RGB, 4=RGBA
    std::string colorSpace;         // Color space name
    std::vector<uint8_t> iccProfile; // ICC profile data
    std::string format;             // Original format
    std::string cameraMake, cameraModel;
    std::string dateTime;
    double exposureTime, fNumber;
    int isoSpeed;
    double focalLength;
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
exports.ImageDecoder             // Class for batch decoding
```

## JavaScript API Layer

`lib/index.js` provides:

1. **Native module loading** - Tries `build/Release/` then `prebuilds/`
2. **Fallback API** - Graceful degradation when native module unavailable
3. **Format detection** - Pure JS implementation for common formats
4. **Error handling** - Helpful messages for setup issues

## Build Process

1. `npm run setup` - Downloads/vendor dependencies
2. `npm run configure` - Runs `node-gyp configure`
3. `npm run build` - Compiles native module

### Windows Build

Uses Visual Studio 2022 (msvs_toolset: v143)

### Linux Build

Uses GCC/Clang with C++17

## Dependencies

### libraw

- Website: https://www.libraw.org/
- License: Licenseer (proprietary, but libraw decoder is GPL)
- Purpose: RAW image decoding

### libheif

- Website: https://github.com/strukturag/libheif
- License: LGPL
- Purpose: HEIC/HEIF/AVIF decoding
- Dependencies: libde265 (for HEVC), aom (for AVIF)

## Performance Considerations

1. **Memory allocation**: ImageData uses std::vector for dynamic pixel buffers
2. **Buffer copies**: NAPI Buffer::Copy used to avoid memory leaks
3. **Format detection**: O(1) magic byte checks, no allocation
4. **Metadata extraction**: Can be done without full demosaicing

## Future Enhancements

- [ ] Full libraw integration with all demosaicing algorithms
- [ ] libheif with AVIF support
- [ ] ICC color profile application
- [ ] Tile-based decoding for large images
- [ ] Streaming decode support
- [ ] Write support (encode to RAW/HEIC)
