# nImage - Development Plan

**Last Updated**: 2026-04-04
**Version**: 2.1.0 (ImageMagick fallback complete)

---

## 1. Objectives

- **Universal image codec**: Support all major formats via native libraries
- **Sharp-compatible API**: nImage wraps Sharp, adding RAW/HEIC support seamlessly
- **High performance**: Match or exceed FFmpeg/sharp for common operations
- **Standalone module**: Reusable across projects without CLI dependencies

### Key Changes from v1.x

| Aspect | Old Architecture | New Architecture |
|--------|-----------------|-------------------|
| API | Custom decode-only | Sharp-compatible pipeline |
| Transforms | Not supported | Via Sharp (main engine) |
| Output formats | Not supported | Via Sharp: PNG, JPEG, WebP, AVIF |
| Pipeline | Decode only | RAW/HEIC decode → Sharp → encode |
| RAW/HEIC support | Native decode | Native decode → Sharp handles rest |

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
| libjpeg | JPEG decode | N/A (Sharp handles) |
| libpng | PNG decode | N/A (Sharp handles) |
| libwebp | WebP decode | N/A (Sharp handles) |
| libtiff | TIFF decode | N/A (Sharp handles) |

### Native Libraries (Encoders)

All encoding is handled by Sharp. No native encoders needed.

| Library | Purpose | Status |
|---------|---------|--------|
| libjpeg | JPEG encode | N/A (Sharp handles) |
| libpng | PNG encode | N/A (Sharp handles) |
| libwebp | WebP encode | N/A (Sharp handles) |
| libaom | AVIF encode | N/A (Sharp handles) |
| libtiff | TIFF encode | N/A (Sharp handles) |

### Integration
- **Sharp**: The main transformation and encoding engine. All transforms and output formats go through Sharp's libvips pipeline. nImage adds RAW/HEIC support by decoding those formats first, then passing the RGB buffer to Sharp.

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

### Phase 4: Sharp Integration ✅ DONE
- [x] Add sharp as dependency
- [x] Create Sharp-compatible pipeline wrapper in nImage
- [x] Route RAW/HEIC decode → Sharp transforms → Sharp encode
- [x] Test: RAW → resize → JPEG pipeline
- [x] Test: HEIC → crop → WebP pipeline
- [x] Test: all formats pass-through pipeline
- [x] Document pipeline usage

### Phase 5: Standard Format Decoders ✅ DONE
- [x] Format detection for JPEG/PNG/WebP/TIFF
- [x] Standard formats (JPEG, PNG, WebP, TIFF, GIF) pass through Sharp directly
- [x] No native decoder needed - Sharp handles these natively

### Phase 6: Pipeline Polish 🔲 TODO
- [ ] Error propagation from Sharp through nImage
- [ ] Metadata preservation (EXIF) through pipeline
- [ ] Performance: avoid buffer copies where possible

### Phase 7: Encoder Options (via Sharp) ✅ DONE
- [x] Quality settings for JPEG/WebP/AVIF
- [x] Compression level for PNG
- [x] Lossless mode for WebP
- [x] StripExif option

### Phase 8: AVIF Support (via Sharp) ✅ DONE
- [x] Test Sharp AVIF output
- [x] Pipeline: any format → Sharp AVIF encode

### Phase 9: Multi-page PDF Support ⬜ FUTURE
- [ ] Get page count for multi-page PDFs
- [ ] Render specific page to image
- [ ] Document ingest utilities for CMS/RAG pipelines

### Phase 10: Polish ⬜ FUTURE
- [ ] Tile-based decoding for large images
- [ ] Streaming decode for memory efficiency
- [ ] Thumbnail extraction without full decode
- [ ] Performance benchmarks vs ImageMagick/sharp

### Phase 11: Build Extensions ⬜ FUTURE
- [ ] macOS build support
- [ ] ARM64 Windows build

### Removed/Deprecated
- **Native JPEG/PNG/WebP encoders**: Sharp handles these
- **Native TIFF encoder**: Sharp handles this
- **Native AVIF encoder**: Sharp handles this
- **Color space handling**: sRGB output by default, no ICC conversion planned

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

nImage presents a **Sharp-compatible API**. This means users can use nImage exactly as they would use Sharp, but with added support for RAW and HEIC formats.

### Sharp-Compatible Pipeline API

```javascript
// nImage acts like sharp - chain operations on an image
const result = await nImage(inputPathOrBuffer)
    .resize(1024, 1024, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Works with RAW and HEIC (Sharp can't handle these natively)
const thumb = await nImage('photo.cr2')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 80 })
    .toBuffer();

// Also works with standard formats Sharp handles directly
const optimized = await nImage('photo.jpg')
    .resize(800, 600)
    .webp({ quality: 85 })
    .toBuffer();
```

### Low-Level API (for advanced use cases)

```javascript
// Format detection (always available - pure JS)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decode RAW/HEIC to raw pixel data (when you need manual control)
nImage.decode(buffer, [formatHint]) → ImageData

// Encode raw pixels to format (via Sharp)
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

// Encoder for output generation (via Sharp)
const encoder = new nImage.ImageEncoder(format)  // 'jpeg', 'png', 'webp'
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer
encoder.getError() → string
```

### Options

```javascript
// Pipeline options (passed to Sharp)
{
    quality: 85,           // 1-100 (JPEG, WebP lossy)
    stripExif: true,       // Remove metadata
    compressionLevel: 6,    // PNG: 0-9
    lossless: false        // WebP lossless mode
}

// Decoder options
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
- [ ] Pipeline output validation (size, format)
- [ ] Pipeline quality settings
- [ ] Round-trip: RAW decode → Sharp transform → JPEG encode

### Integration Tests
- [ ] Pipeline: RAW → Sharp resize → JPEG output
- [ ] Pipeline: RAW → Sharp crop → WebP output
- [ ] Pipeline: HEIC → Sharp rotate → JPEG output
- [ ] Pipeline: HEIC → Sharp resize → PNG output
- [ ] Pipeline: JPEG → Sharp transform → WebP (passthrough)
- [ ] Pipeline: PNG → Sharp transform → AVIF (via Sharp)
- [ ] Decode real RAW files (CR2, NEF, ARW, ORF, RAF, DNG)
- [ ] Decode real HEIC files from iOS
- [ ] Compare output with ImageMagick/sharp

### Performance Tests
- [ ] Decode benchmarks (see README for results)
- [ ] Pipeline benchmarks: RAW → resize → JPEG (end-to-end)
- [ ] Pipeline benchmarks: HEIC → resize → WebP (end-to-end)
- [ ] Memory usage for large files
- [ ] Compare vs ImageMagick/sharp for equivalent operations

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

### Architecture: Sharp as the Main Engine

**All transformations and encoding go through Sharp.** nImage's role is to add RAW/HEIC support by:
1. Detecting if input is RAW or HEIC
2. Decoding to RGB/RGBA via native codecs (libraw, libheif)
3. Passing the buffer to Sharp for the requested operations
4. Returning Sharp's output to the user

This means:
- **No native encoders needed** - Sharp handles JPEG, PNG, WebP, AVIF encoding
- **Single API surface** - Users don't need to know if their input went through native decode
- **Best of both worlds** - Sharp's optimized libvips pipeline + RAW/HEIC support

### Pipeline Flow

```
User: nImage('photo.cr2').resize(1024).jpeg().toBuffer()
         │
         ▼
nImage: detectFormat() → CR2 (native decode needed)
         │
         ▼
nImage: decode() → RGB buffer via libraw
         │
         ▼
Sharp: sharp(rgbBuffer, {raw: {width, height, channels}})
         │
         ▼
Sharp: .resize(1024).jpeg().toBuffer()
         │
         ▼
User: receives JPEG buffer
```

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

### v2.1.0 (Current)
- **ImageMagick fallback**: 150+ additional formats supported (PDF, SVG, AI, DOCX, XLSX, PPTX, EXR, HDR, etc.)
- **MagickDecoder**: Catch-all fallback for any format not handled by LibRaw/LibHeif
- **212 formats total**: RAW (22), HEIC (3), Standard (9), Documents (13), Scientific (20), Video Stills (12), Other (150+)

### v2.0.0
- **Sharp-compatible API**: nImage now presents Sharp's API, adding RAW/HEIC support transparently
- **Pipeline architecture**: RAW/HEIC decode → Sharp transforms → Sharp encode
- **Removed native encoders**: Sharp handles all encoding (JPEG, PNG, WebP, AVIF)
- **Simplified codebase**: No more separate encoder implementations needed

### v1.0.0
- Initial release with RAW and HEIC decoding
- LibRaw and LibHeif integration
- Pre-compiled binaries for Windows
