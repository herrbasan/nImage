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
- **Custom build script**: Direct g++ invocation with MSYS2 MinGW

### Native Libraries

| Library | Purpose | License | Status |
|---------|---------|---------|--------|
| libraw | RAW decoding | LGPL/GPL | ✅ Working |
| libheif | HEIC/HEIF/AVIF | LGPL | ✅ Working |
| libde265 | HEVC decoding (for HEIC) | LGPL | ✅ Via MSYS2 |
| aom | AV1 decoding (for AVIF) | BSD | ✅ Via MSYS2 |
| LittleCMS | ICC color management | MIT | ⬜ Future |

### Dependencies
- libraw 0.22.0 via MSYS2 (mingw-w64-x86_64-libraw)
- libheif via MSYS2 (mingw-w64-x86_64-libheif)
- All transitive dependencies via MSYS2 (51 DLLs)

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
- [ ] Verify color accuracy vs ImageMagick (future)

### Phase 3: LibHeif Integration ✅ DONE
- [x] Implement LibHeifDecoder class
- [x] heif_context_alloc()
- [x] heif_context_read_from_memory_without_copy()
- [x] heif_context_get_primary_image_handle()
- [x] heif_image_decode() to RGB/RGBA
- [ ] Add AVIF support via aom (should work, untested)
- [x] Test with HEIC samples from iOS devices
- [ ] Verify thumbnail extraction works

### Phase 4: Color Management ⬜ FUTURE
- [ ] Integrate LittleCMS for ICC profile handling
- [ ] Apply ICC profiles to decoded images
- [ ] Support color space conversion (sRGB, AdobeRGB, Display P3)
- [ ] Profile embedding in output

### Phase 5: Polish & Performance ⬜ FUTURE
- [ ] JPEG encoding (output to JPEG)
- [ ] Tile-based decoding for large images
- [ ] Streaming decode for memory efficiency
- [ ] Thumbnail extraction without full decode
- [ ] Progressive JPEG-like loading
- [ ] Performance benchmarks vs ImageMagick

---

## 4. Build System

### Windows Build (MSYS2 + Direct g++)

The build uses a custom build script (`scripts/build.js`) that directly invokes g++ from MSYS2 instead of relying on node-gyp (which defaults to MSVC on Windows even with `--compiler=mingw`).

```powershell
# Full setup
.\scripts\setup.ps1

# Or build directly (if deps already present)
cd modules/nImage
npm run build
```

The build script:
1. Sets up MSYS2 environment (MSYSTEM=MINGW64)
2. Locates Node.js headers from node-gyp cache
3. Creates import library from node.lib for NAPI linking
4. Invokes g++ with proper include/library paths
5. Copies artefact and DLLs to dist/

### Linux Build
```bash
# Install libraw and libheif via package manager
sudo apt install libraw-dev libheif-dev

# Build
npm run build
```

### Dependencies Installation

**Via MSYS2 pacman (Windows - recommended):**
```bash
pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif
```

---

## 5. API Specification

### Core Functions

```javascript
// Detect format from buffer (pure JS, always available)
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decode image to pixel data (native when available)
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
  camera: { make, model };
  capture: { exposureTime, fNumber, isoSpeed, focalLength };
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
- Decode real HEIC files (from iOS devices) - working
- Compare output with ImageMagick/dcraw

### Performance Tests
- Decode time benchmarks (see README for results)
- Memory usage for large files
- Tile-based vs full decode benchmarks

### Test Files Available
- [x] CR2 (Canon) - IMG_2593.CR2
- [x] ORF (Olympus) - _4260446.ORF
- [x] HEIC (Apple) - IMG_0092_1.HEIC
- [ ] NEF (Nikon) - need sample
- [ ] ARW (Sony) - need sample
- [ ] RAF (Fujifilm) - need sample
- [ ] AVIF - need sample

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

## 8. Known Issues

1. **JPEG encoding not available**: Decode outputs pixel data but no encoder to write JPEG
2. **shot_select is unsigned in LibRaw 0.22**: Code was fixed to always open buffer on each decode

---

## 9. Future Enhancements

- [ ] **Encoding support** - Write to JPEG/PNG/HEIC formats
- [ ] **Thumbnail extraction** - Fast preview without full decode
- [ ] **EXIF manipulation** - Modify metadata
- [ ] **Batch processing** - Decode multiple images efficiently
- [ ] **WebAssembly build** - Cross-platform without native compilation
- [ ] **Zero-copy decode** - Direct GPU access for processing

---

## 10. Repository

**URL**: https://github.com/herrbasan/nImage

**Structure**:
```
nImage/
├── src/           # C++ source
├── lib/           # JavaScript entry point
├── dist/          # Pre-compiled binaries (tracked in git)
├── deps/          # Build dependencies (gitignored)
├── scripts/       # Build/setup scripts
├── docs/          # Documentation
├── test/          # Test files and benchmarks
└── examples/      # Usage examples (future)
```
