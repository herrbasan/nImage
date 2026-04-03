# nImage - Development Plan

**Last Updated**: 2026-04-03
**Version**: 1.0.0 → 2.0.0 (major redesign)

---

## 1. Objectives

- **Universal image codec**: Support all major formats via native libraries
- **Codec-first design**: Decode exotic formats (RAW, HEIC), delegate transforms to Sharp
- **High performance**: Match or exceed FFmpeg/sharp for common operations
- **Standalone module**: Reusable across projects without CLI dependencies

### Key Changes from v1.x

| Aspect | Old Architecture | New Architecture |
|--------|-----------------|-------------------|
| JPEG/PNG decode | Via FFmpeg | Via native libs |
| Transforms | Not supported | Via Sharp |
| Output formats | Not supported | PNG, JPEG, WebP, AVIF |
| Pipeline | Decode only | Decode → Transform → Encode |

---

## 2. Technology Stack

### Core
- **Node.js 18+**: NAPI bindings, JavaScript API layer
- **node-addon-api**: NAPI C++ wrapper (official)
- **Custom build script**: Direct g++ invocation with MSYS2 MinGW

### Native Libraries (Decoders)

| Library | Purpose | Status |
|---------|---------|--------|
| libraw | RAW decoding | ✅ Working |
| libheif | HEIC/HEIF/AVIF | ✅ Working |
| libjpeg | JPEG decode | ⬜ Future |
| libpng | PNG decode | ⬜ Future |
| libwebp | WebP decode | ⬜ Future |
| libtiff | TIFF decode | ⬜ Future |

### Native Libraries (Encoders)

| Library | Purpose | Status |
|---------|---------|--------|
| libjpeg | JPEG encode | 🔲 TODO |
| libpng | PNG encode | 🔲 TODO |
| libwebp | WebP encode | 🔲 TODO |
| libaom | AVIF encode | 🔲 TODO |
| libtiff | TIFF encode | ⬜ Future |

### Integration
- **Sharp**: Transformation pipeline (resize, crop, rotate, composite)

---

## 3. Development Phases

### Phase 1: Foundation ✅ DONE
- [x] Module structure and package.json
- [x] NAPI binding layer skeleton
- [x] Base decoder class with factory pattern
- [x] Format detection (magic bytes)
- [x] JavaScript API layer with graceful fallback
- [x] Build infrastructure (custom build.js using direct g++)
- [x] Pre-compiled binaries in dist/ folder
- [x] Documentation (README, spec)

### Phase 2: LibRaw Integration ✅ DONE
- [x] MSYS2 package installation (libraw 0.22.0)
- [x] LibRawDecoder class implementation
- [x] open_buffer() for in-memory RAW files
- [x] unpack() for decoding
- [x] dcraw_process() for demosaicing
- [x] Metadata extraction (EXIF, camera info, capture settings)
- [x] LibRaw 0.22 API compatibility fixes
- [x] Test with CR2, NEF, ARW, ORF samples

### Phase 3: LibHeif Integration ✅ DONE
- [x] Implement LibHeifDecoder class
- [x] heif_context_alloc()
- [x] heif_context_read_from_memory_without_copy()
- [x] heif_context_get_primary_image_handle()
- [x] heif_image_decode() to RGB/RGBA
- [x] AVIF support via aom (should work, untested)
- [x] Test with HEIC samples from iOS devices

### Phase 4: Standard Format Decoders 🔲 TODO
- [x] Format detection for JPEG/PNG/WebP/TIFF
- [ ] JpegDecoder class (libjpeg)
- [ ] PngDecoder class (libpng)
- [ ] WebPDecoder class (libwebp)
- [ ] TiffDecoder class (libtiff)
- [ ] Integrate decode into NAPI bindings

### Phase 5: Encoder Foundation 🔲 TODO
- [ ] Create encoder.h with base class
- [ ] Implement EncoderFactory
- [ ] Add EncoderOptions struct
- [ ] Update NAPI bindings for encode function

### Phase 6: JPEG Encoder 🔲 TODO
- [ ] Create JpegEncoder class
- [ ] IJG compression setup (jpeg_compress_struct)
- [ ] Quality and color space options
- [ ] Error handling for invalid input
- [ ] Link with libjpeg from dist/
- [ ] Add unit tests
- [ ] Benchmark vs sharp

### Phase 7: PNG Encoder 🔲 TODO
- [ ] Create PngEncoder class
- [ ] libpng setup (png_struct, info_struct)
- [ ] Compression level (0-9)
- [ ] Color type handling (RGB, RGBA)
- [ ] Add unit tests

### Phase 8: WebP Encoder 🔲 TODO
- [ ] Create WebPEncoder class
- [ ] Lossy and lossless modes
- [ ] Quality parameter
- [ ] RGBA support
- [ ] Add unit tests

### Phase 9: AVIF Encoder (via Sharp) 🔲 TODO
- [ ] Test Sharp AVIF output
- [ ] Evaluate libaom encoder (complex, slow)
- [ ] If native needed: implement avif_encoder.cpp
- [ ] Benchmark vs libvips

### Phase 10: Sharp Integration 🔲 TODO
- [ ] Add sharp as dependency
- [ ] Document pipeline usage
- [ ] Create helper for raw buffer → sharp
- [ ] Example: RAW → resize → JPEG pipeline
- [ ] Example: HEIC → crop → WebP pipeline

### Phase 11: Color Management ⬜ FUTURE
- [ ] Integrate LittleCMS for ICC profile handling
- [ ] Apply ICC profiles to decoded images
- [ ] Support color space conversion
- [ ] Profile embedding in output

### Phase 12: Polish ⬜ FUTURE
- [ ] Tile-based decoding for large images
- [ ] Streaming decode for memory efficiency
- [ ] Thumbnail extraction without full decode
- [ ] Performance benchmarks vs ImageMagick/sharp
- [ ] macOS build support
- [ ] ARM64 Windows build

---

## 4. Build System

### Windows Build (MSYS2 + Direct g++)

```powershell
# Full setup (installs MSYS2, deps, builds)
.\scripts\setup.ps1

# Or skip install (if deps already present)
.\scripts\setup.ps1 -SkipInstall

# Just build
npm run build
```

### Linux Build

```bash
# Install dependencies
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev libtiff-dev

# Build
npm run build
```

### Dependencies Installation

**Via MSYS2 pacman (Windows):**
```bash
pacman -S mingw-w64-x86_64-libraw \
          mingw-w64-x86_64-libheif \
          mingw-w64-x86_64-libjpeg-turbo \
          mingw-w64-x86_64-libpng \
          mingw-w64-x86_64-libwebp \
          mingw-w64-x86_64-libtiff \
          mingw-w64-x86_64-aom
```

---

## 5. API Specification

### Core Functions

```javascript
// Format detection (always available)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decode to raw pixel data
nImage.decode(buffer, [formatHint]) → ImageData

// Encode from raw pixel data
nImage.encode(rgbBuffer, width, height, channels, format, options) → Buffer

// Get supported formats
nImage.getSupportedFormats() → string[]

// Check if native module loaded
nImage.isLoaded → boolean
```

### Classes

```javascript
// Decoder for batch operations
const decoder = new nImage.ImageDecoder([format])
decoder.decode(buffer) → ImageData
decoder.getMetadata(buffer) → ImageMetadata
decoder.getError() → string

// Encoder for output generation
const encoder = new nImage.ImageEncoder(format)  // 'jpeg', 'png', 'webp'
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer
encoder.getError() → string
```

### Options

```javascript
// Encoder options
{
    quality: 85,           // 1-100 (JPEG, WebP lossy)
    stripExif: true,       // Remove metadata
    compressionLevel: 6,    // PNG: 0-9
    lossless: false        // WebP lossless mode
}

// Decoder options (future)
{
    applyOrientation: true,
    colorSpace: 'sRGB'
}
```

---

## 6. Return Types

### ImageData (Decode Output)

```typescript
interface ImageData {
    width: number;
    height: number;
    bitsPerChannel: number;
    channels: number;       // 3=RGB, 4=RGBA
    colorSpace: string;
    format: string;
    hasAlpha: boolean;
    data: Buffer | null;   // Raw pixel data
    iccProfile: Buffer | null;
    camera: { make: string; model: string };
    capture: {
        dateTime: string;
        exposureTime: number;
        fNumber: number;
        isoSpeed: number;
        focalLength: number;
    };
    raw: { width: number; height: number };
    orientation: number;
}
```

### ImageMetadata (Lightweight)

```typescript
interface ImageMetadata {
    format: string;
    width: number;
    height: number;
    rawWidth: number;
    rawHeight: number;
    hasAlpha: boolean;
    bitsPerSample: number;
    orientation: number;
    fileSize: number;
    camera: { make: string; model: string };
    capture: {
        isoSpeed: number;
        exposureTime: number;
        fNumber: number;
        focalLength: number;
        lensModel: string;
    };
    colorSpace: string;
}
```

---

## 7. Testing Strategy

### Unit Tests
- [x] Format detection (magic byte signatures)
- [x] API surface (all functions callable)
- [x] Error handling (invalid inputs)
- [ ] Encoder output validation (size, format)
- [ ] Encoder quality settings
- [ ] Round-trip: decode → encode → decode

### Integration Tests
- [ ] Decode real RAW files (CR2, NEF, ARW, ORF, RAF, DNG)
- [ ] Decode real HEIC files from iOS
- [ ] Decode standard formats (JPEG, PNG, WebP, TIFF)
- [ ] Encode to JPEG/PNG/WebP
- [ ] Compare output with ImageMagick
- [ ] Pipeline: RAW → Sharp resize → encode
- [ ] Pipeline: HEIC → Sharp crop → encode

### Performance Tests
- [ ] Decode benchmarks (see README for results)
- [ ] Encode benchmarks (JPEG, PNG, WebP)
- [ ] Memory usage for large files
- [ ] Tile-based vs full decode benchmarks

### Test Files Available
- [x] CR2 (Canon) - IMG_2593.CR2
- [x] ORF (Olympus) - _4260446.ORF
- [x] HEIC (Apple) - IMG_0092_1.HEIC
- [ ] NEF (Nikon) - need sample
- [ ] ARW (Sony) - need sample
- [ ] RAF (Fujifilm) - need sample
- [ ] AVIF - need sample

---

## 8. Error Handling

| Error | Cause | Recovery |
|-------|-------|----------|
| `UNSUPPORTED_FORMAT` | Unknown/unsupported format | Suggest supported formats |
| `DECODE_FAILED` | Corrupt file or missing codec | Check error message |
| `ENCODE_FAILED` | Invalid parameters | Check format/quality values |
| `OUT_OF_MEMORY` | Image too large | Suggest tile-based decode |
| `MODULE_NOT_LOADED` | Native lib not built | Run `npm run setup` |

---

## 9. Implementation Notes

### Encoder Implementation Order

1. **JPEG** - Most used, straightforward API, good test bed
2. **PNG** - Similar pattern to JPEG, different lib
3. **WebP** - Similar pattern, lossy/lossless mode
4. **AVIF** - Complex, use Sharp first, evaluate native later

### Sharp Integration Pattern

```javascript
// For RAW/HEIC input, Sharp needs raw RGB buffer
const imageData = nImage.decode(buffer);

// Convert to Sharp-compatible input
const sharpInput = {
    raw: {
        width: imageData.width,
        height: imageData.height,
        channels: imageData.channels  // 3=RGB, 4=RGBA
    }
};

// Transform with Sharp
const transformed = await sharp(Buffer.from(imageData.data), sharpInput)
    .resize(1024, 1024, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();
```

### Performance Considerations

- Individual encode/decode should match native libs
- Sharp wins on chained operations (libvips lazy evaluation)
- AVIF encoding is inherently slow (AV1 codec) - no way around it

---

## 10. Repository

**URL**: https://github.com/herrbasan/nImage

**Clone for standalone development**:
```bash
git clone https://github.com/herrbasan/nImage.git
cd nImage
npm run setup
```

---

## 11. Changelog

### v2.0.0 (Planned)
- Added encoder support (JPEG, PNG, WebP)
- Added Sharp integration for transformations
- Standard format decoders (JPEG, PNG, WebP, TIFF)
- Complete codec pipeline: decode → transform → encode

### v1.0.0
- Initial release with RAW and HEIC decoding
- LibRaw and LibHeif integration
- Pre-compiled binaries for Windows
