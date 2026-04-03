# nImage - Development Plan

Native image decoding via NAPI for Node.js with focus on RAW and HEIC formats.

---

## 1. Objectives

- **Native NAPI bindings** for image decoding (no CLI overhead)
- **RAW format support** via libraw (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
- **HEIC/HEIF support** via libheif (Apple's modern image format)
- **ICC color management** for accurate color reproduction
- **Reusable module** across projects (MediaService, other Node.js apps)

---

## 2. Technology Stack

### Core
- **Node.js 18+**: NAPI bindings, JavaScript API layer
- **node-addon-api**: NAPI C++ wrapper (official)
- **node-gyp**: Build system

### Native Libraries

| Library | Purpose | License | Status |
|---------|---------|--------|--------|
| libraw | RAW decoding | LGPL/GPL | ✅ Stub complete, implementation pending |
| libheif | HEIC/HEIF/AVIF | LGPL | ✅ Stub complete, implementation pending |
| libde265 | HEVC decoding (for HEIC) | LGPL | ✅ Via MSYS2 |
| aom | AV1 decoding (for AVIF) | BSD | ✅ Via MSYS2 |
| LittleCMS | ICC color management | MIT | ⬜ Future |

### Dependencies
- libraw depends on: dcraw compatibility, color matrices (via MSYS2)
- libheif depends on: libde265 (HEVC), aom (AVIF), libjpeg, libpng (all via MSYS2)

---

## 3. Development Phases

### Phase 1: Foundation ✅ DONE
- [x] Module structure and package.json
- [x] NAPI binding layer skeleton
- [x] Base decoder class with factory pattern
- [x] Format detection (magic bytes)
- [x] JavaScript API layer with graceful fallback
- [x] Build infrastructure (binding.gyp, vendor.js)
- [x] Documentation (README, spec)

### Phase 2: LibRaw Integration
- [ ] Obtain libraw binaries/headers (vcpkg or source)
- [ ] Implement LibRawDecoder class
  - [ ] open_buffer() for in-memory RAW files
  - [ ] unpack() for decoding
  - [ ] dcraw_process() for demosaicing
  - [ ] extract metadata (EXIF, camera info)
- [ ] Integrate ICC profile extraction
- [ ] Test with CR2, NEF, ARW samples
- [ ] Verify color accuracy vs ImageMagick

### Phase 3: LibHeif Integration
- [ ] Obtain libheif binaries/headers (vcpkg or source)
- [ ] Implement LibHeifDecoder class
  - [ ] heif_context_alloc()
  - [ ] heif_read_from_memory()
  - [ ] heif_context_get_primary_image_handle()
  - [ ] heif_image_decode()
- [ ] Add AVIF support via aom
- [ ] Test with HEIC samples from iOS devices
- [ ] Verify thumbnail extraction works

### Phase 4: Color Management
- [ ] Integrate LittleCMS for ICC profile handling
- [ ] Apply ICC profiles to decoded images
- [ ] Support color space conversion (sRGB, AdobeRGB, Display P3)
- [ ] Profile embedding in output

### Phase 5: Polish & Performance
- [ ] Tile-based decoding for large images
- [ ] Streaming decode for memory efficiency
- [ ] Thumb nail extraction without full decode
- [ ] Progressive JPEG-like loading
- [ ] Performance benchmarks vs ImageMagick

---

## 4. Build System

### Windows (MSYS2 - recommended)
```bash
# Install MSYS2 from https://www.msys2.org/
# In MSYS2 MINGW64 terminal:
pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif

# Back in regular terminal:
npm run setup    # Copies MSYS2 deps to deps/
npm run build    # Builds using MinGW
```

### Linux
```bash
# Install libraw and libheif via package manager
sudo apt install libraw-dev libheif-dev
npm run setup
npm run build
```

### Dependencies Installation

**Via MSYS2 pacman (Windows - recommended):**
```bash
pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif
```

**Via vcpkg:**
```bash
vcpkg install libraw:x64-windows libheif:x64-windows
# Note: requires manual copy to deps/ structure
```

**Or manually:**
1. Download prebuilt binaries or build from source
2. Copy headers to `deps/include/`
3. Copy libraries to `deps/lib/`
4. Copy DLLs to `deps/bin/`

---

## 5. API Specification

### Core Functions

```javascript
// Detect format from buffer (pure JS, always available)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decode image to pixel data
nImage.decode(buffer, [formatHint]) → ImageData

// Get supported format list
nImage.getSupportedFormats() → string[]
```

### ImageDecoder Class

```javascript
const decoder = new nImage.ImageDecoder([format])
decoder.decode(buffer) → ImageData
decoder.getMetadata(buffer) → ImageMetadata
decoder.getError() → string
```

### Return Types

```typescript
interface ImageData {
  width: number;              // Decoded width
  height: number;             // Decoded height
  bitsPerChannel: number;     // 8 or 16
  channels: number;           // 3=RGB, 4=RGBA
  colorSpace: string;         // 'sRGB', 'Adobe RGB', etc.
  format: string;             // Original format
  hasAlpha: boolean;
  data: Buffer | null;        // Pixel data
  iccProfile: Buffer | null;
  camera: { make, model };
  capture: { dateTime, exposureTime, fNumber, isoSpeed, focalLength };
  raw: { width, height };     // Sensor dimensions
  orientation: number;        // EXIF orientation 1-8
}

interface ImageMetadata {
  format: string;
  width, height: number;
  rawWidth, rawHeight: number;
  camera: { make, model };
  capture: { isoSpeed, exposureTime, fNumber, focalLength };
  colorSpace: string;
  hasAlpha: boolean;
  bitsPerSample: number;
  orientation: number;
  fileSize: number;
}
```

---

## 6. Testing Strategy

### Unit Tests
- Format detection (magic byte signatures)
- API surface (all functions callable)
- Error handling (invalid inputs)

### Integration Tests
- Decode real RAW files (CR2, NEF, ARW, ORF, RAF, DNG)
- Decode real HEIC files (from iOS devices)
- Compare output with ImageMagick/dcraw

### Performance Tests
- Decode time vs ImageMagick
- Memory usage for large files
- Tile-based vs full decode benchmarks

### Test Files Needed
- [ ] CR2 (Canon)
- [ ] NEF (Nikon)
- [ ] ARW (Sony)
- [ ] ORF (Olympus)
- [ ] RAF (Fujifilm)
- [ ] RW2 (Panasonic)
- [ ] DNG (Adobe)
- [ ] HEIC (Apple iOS)
- [ ] AVIF (various)

---

## 7. Error Handling

| Error | Cause | Recovery |
|-------|-------|----------|
| `UNSUPPORTED_FORMAT` | Unknown/unsupported format | Suggest supported formats |
| `DECODE_FAILED` | Corrupt file or missing codec | Check error message |
| `OUT_OF_MEMORY` | Image too large | Suggest tile-based decode |
| `ICC_ERROR` | Invalid ICC profile | Skip ICC, return raw |
| `DEPENDENCY_MISSING` | Native lib not loaded | Run npm run setup |

---

## 8. Future Enhancements

- [ ] **Encoding support** - Write to RAW/HEIC formats
- [ ] **Thumbnail extraction** - Fast preview without full decode
- [ ] **EXIF manipulation** - Modify metadata
- [ ] **Batch processing** - Decode multiple images efficiently
- [ ] **WebAssembly build** - Cross-platform without native compilation
- [ ] **Zero-copy decode** - Direct GPU access for processing

---

## 9. Repository

**URL**: https://github.com/herrbasan/nImage

**Structure**:
```
nImage/
├── src/           # C++ source
├── lib/           # JavaScript entry point
├── deps/          # Vendored dependencies (gitignored)
├── scripts/       # Build/setup scripts
├── docs/          # Documentation
├── test/          # Test files
└── examples/      # Usage examples (future)
```
