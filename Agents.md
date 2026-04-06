# nImage - Agent Development Guide

**Project**: Native image codec for Node.js via NAPI
**Spec**: [docs/nImage_spec.md](docs/nImage_spec.md)
**Dev Plan**: [docs/nImage_dev_plan.md](docs/nImage_dev_plan.md)

## Core Development Maxims

**Priorities: Reliability > Performance > Everything else.**

**LLM-Native Codebase**: Code readability and structure for humans is a non-goal. The code will not be maintained by humans. Optimize for the most efficient structure an LLM can understand. Do not rely on conventional human coding habits.

**Vanilla JS**: No TypeScript anywhere. Code must stay as close to the bare platform as possible for easy optimization and debugging. `.d.ts` files are generated strictly for LLM/editor context, not used at runtime.

**Zero Dependencies**: If we can build it ourselves using raw standard libraries, we build it. Avoid external third-party packages. Evaluate per-case if a dependency is truly necessary.

**Fail Fast, Always**: No defensive coding. No mock data, no fallback defaults, and no silencing try/catch blocks. The goal is to write perfect, deterministic software. When it breaks, let it crash and fix the root cause.

---

## Architecture

```
nImage/
├── src/
│   ├── decoder.h           # Base decoder class, ImageData/ImageMetadata structs
│   ├── decoder.cpp         # LibRawDecoder, LibHeifDecoder implementations
│   ├── encoder.h           # Base encoder class, EncoderOptions struct
│   ├── encoder.cpp         # Encoder implementations (TODO)
│   └── binding.cpp          # NAPI bindings - JS entry point
├── lib/
│   └── index.js            # JS API layer with graceful fallback
├── dist/                   # Pre-compiled Windows binaries (minimal DLLs, magick.exe + nimage.node)
├── scripts/
│   ├── setup.ps1           # Full setup: MSYS2, deps, build, test
│   └── build.js             # Direct g++ invocation
└── test/
    ├── index.test.js       # Unit tests
    ├── benchmark.js        # Performance benchmarks
    └── assets/              # Test images (CR2, ORF, HEIC samples)
```

## Key Structures

### ImageData (decoder output)
- `data`: `vector<uint8_t>` - Raw RGB/RGBA pixel data
- `width`, `height`: Decoded image dimensions
- `channels`: 3=RGB, 4=RGBA
- `bitsPerChannel`: 8 or 16
- `colorSpace`: sRGB, AdobeRGB, etc.
- `iccProfile`: Optional ICC color profile
- `camera` / `capture`: EXIF metadata
- `orientation`: EXIF orientation 1-8

### ImageFormat enum
```
UNKNOWN, CR2, NEF, ARW, ORF, RAF, RW2, DNG, PEFR, SRW, RWL,
HEIC, HEIF, AVIF, JPEG, PNG, TIFF, WEBP, GIF, BMP, JXL, JP2,
PSD, PDF, SVG, AI, DOC/DOCX, XLS/XLSX, PPT/PPTX, EPS, XPS,
EXR, HDR, BIGTIFF, CIN, DPX, FITS, FLIF, MIFF, MPC, PCD, PFM,
SGI, TGA, VTF, and 150+ additional formats via ImageMagick
```

## Decoders

### LibRawDecoder
- RAW formats: CR2, NEF, ARW, ORF, RAF, RW2, DNG, PEFR, SRW, RWL, CRW, MRW, NRW, etc.
- Uses `open_buffer()` for in-memory decoding
- `dcraw_process()` for demosaicing
- Extracts full EXIF metadata

### LibHeifDecoder
- HEIC, HEIF, AVIF formats
- `heif_context_read_from_memory_without_copy()`
- Outputs RGB or RGBA depending on alpha

### MagickDecoder (Fallback CLI)
- Handles all formats not covered by LibRaw/LibHeif and Sharp inside the JS wrapper `lib/index.js`
- Uses `magick.exe` CLI process directly, extracting to PNG to prevent DLL collisions with Electron apps
- Supports 150+ formats: documents (PDF, SVG, AI, DOCX, XLSX, PPTX), scientific (EXR, HDR, DPX, FITS), video stills (AVI, MOV, MP4, MKV), and more

### Thumbnail Extraction (v2.2.0)

Fast thumbnail extraction without full decode:

| Format Type | Method | Speed |
|-------------|--------|-------|
| RAW formats | Native `unpack_thumb()` | ✅ Fast |
| HEIC/HEIF/AVIF | Native `get_thumbnail()` | ✅ Fast |
| JPEG/PNG/WebP/GIF/TIFF/BMP/AVIF | Sharp `resize()` | ✅ Fast |
| Everything else | MagickDecoder resize | ⚠️ Slow |

**Note:** Standard formats (JPEG, PNG, etc.) go through Sharp for thumbnails, NOT MagickDecoder. Only formats Sharp can't handle at all go through MagickDecoder.

## Build System

### Windows
```powershell
.\scripts\setup.ps1              # Full setup (MSYS2 + deps + build + test)
.\scripts\setup.ps1 -SkipInstall  # Skip MSYS2 install, just build
npm run build                     # Just compile
npm run build:debug              # Debug symbols
```

### Linux
```bash
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev libtiff-dev
npm run build
```

### Important: Recompiling After Native Code Changes

**ALWAYS rebuild after modifying C++ source files** (`src/*.cpp`, `src/*.h`). The JS wrapper loads binaries in this order:
1. `build/Release/nimage.node` (development)
2. `prebuilds/nimage.node` (installed package)
3. `dist/nimage.node` (distribution)

**Recommended workflow for Electron apps:**

```powershell
# 1. Clean old binaries (important!)
rm build/Release/nimage.node

# 2. Rebuild for Electron
npx electron-rebuild -f -w nimage -v <electron-version>
# Example: npx electron-rebuild -f -w nimage -v 41.1.1

# 3. Copy to dist (where Electron will find it)
cp build/Release/nimage.node dist/
cp build/Release/*.dll dist/    # Copy all runtime DLLs too
```

**Why delete build/Release first?**
The JS wrapper tries `build/Release` before `dist`. If an old binary exists there, it will be loaded instead of your new one.

**Verify the right binary is loaded:**
```javascript
// In your app, check which binary was loaded:
const path = require('path');
const fs = require('fs');
const buildPath = path.join(__dirname, 'nImage/build/Release/nimage.node');
const distPath = path.join(__dirname, 'nImage/dist/nimage.node');

console.log('build/Release:', fs.existsSync(buildPath) ? fs.statSync(buildPath).mtime : 'not found');
console.log('dist:', fs.statSync(distPath).mtime);
```

**Common mistake**: Forgetting to delete `build/Release/nimage.node` → old binary keeps loading.

## Development Phases (per dev plan)

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ DONE | Foundation: NAPI bindings, base decoder, format detection |
| 2 | ✅ DONE | LibRaw integration for RAW formats |
| 3 | ✅ DONE | LibHeif integration for HEIC/HEIF |
| 4 | ✅ DONE | Sharp integration for transforms and encoding |
| 5 | ✅ DONE | Standard format decoders (Sharp pass-through) |
| 6 | ✅ DONE | Encoder options (via Sharp) |
| 7 | ✅ DONE | AVIF support (via Sharp) |
| 8 | 🔄 IN PROGRESS | Memory & Performance: error propagation ✅, zero-copy ✅, thumbnails ✅, streaming ✅*, Benchmarks ✅* |
* Partially complete - streaming decode and full benchmarks remain
| 9 | ⬜ FUTURE | Multi-page PDF support |
| 10 | ⬜ FUTURE | Large image support: tile-based decoding for 100MP+ images |


**Note:** Encoding is handled by Sharp (JPEG, PNG, WebP, AVIF, TIFF). No separate encoders needed.

## Adding a New Encoder

1. **Add to encoder.h**:
   - Add `EncoderOptions` struct with quality, stripExif, compressionLevel, lossless
   - Create `EncoderBase` class with virtual `encode()` method
   - Add factory method `createEncoder(format)`

2. **Implement in encoder.cpp**:
   - `JpegEncoder` / `PngEncoder` / `WebPEncoder` classes
   - Implement virtual `encode()` with format-specific library calls
   - Populate error_ on failure

3. **Wire in binding.cpp**:
   - Add `EncoderWrapper` NAPI class (see `ImageDecoderWrapper` pattern)
   - Add `EncodeImage` function
   - Register in `Init()`: `exports.Set("ImageEncoder", ...)` and `exports.Set("encode", ...)`

4. **Update lib/index.js**:
   - Add `encode()` function
   - Add `ImageEncoder` class

5. **Test**:
   - Add unit tests in `test/index.test.js`
   - Run `npm test`
   - Benchmark against sharp

## API Reference

```javascript
// Detection
nImage.detectFormat(buffer) → { format, confidence, mimeType }

// Decoding
nImage.decode(buffer, [formatHint]) → ImageData
decoder = new nImage.ImageDecoder([format])
decoder.decode(buffer) → ImageData
decoder.getMetadata(buffer) → ImageMetadata

// Thumbnail extraction (fast, no full decode)
nImage.thumbnail(buffer, { size: 256 }) → ImageData
nImage('photo.cr2').thumbnail({ size: 256 }) → ImageData

// Encoding (TODO)
nImage.encode(rgbBuffer, width, height, channels, format, options) → Buffer
encoder = new nImage.ImageEncoder('jpeg')
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer
```

## Quality Settings and Demosaic Algorithms

LibRaw supports multiple demosaic algorithms with different quality/speed tradeoffs. The `quality` option controls both the demosaic algorithm and processing parameters.

### Quality Presets

```javascript
// Quality: 0 (Draft) - Linear interpolation, minimal processing
nImage.decode(buffer, { format: 'cr2', quality: 0 });
// Fastest option. Good for ultra-fast previews when combined with halfSize.

// Quality: 1 (Fast) - PPG demosaic
nImage.decode(buffer, { format: 'cr2', quality: 1 });
// Balanced speed/quality. Good default for previews.

// Quality: 2 (Balanced) - AHD demosaic  
nImage.decode(buffer, { format: 'cr2', quality: 2 });
// Higher quality demosaic. Best for half-size previews.

// Quality: 3 (Best) - AHD with full processing
nImage.decode(buffer, { format: 'cr2', quality: 3 });
// Maximum quality with highlight reconstruction. For final export.
```

### Performance by Resolution

**Full Resolution (3476×5208):**

| Quality | Algorithm | Time | Speedup | Use Case |
|---------|-----------|------|---------|----------|
| 0 | Linear | 630 ms | **2.7×** | Fast full-res preview |
| 1 | PPG | 707 ms | 2.4× | Balanced full-res |
| 2 | AHD | 1693 ms | 1.0× | High quality full-res |
| 3 | AHD+ | 1719 ms | baseline | Maximum quality export |

**Half Resolution (1738×2604):**

| Quality | Algorithm | Time | Notes |
|---------|-----------|------|-------|
| 0-2 | All | ~370 ms | Algorithm doesn't matter - LibRaw uses simple interpolation when half_size=1 |

### Key Insight

When `halfSize: true` is set, LibRaw automatically skips complex demosaic algorithms and uses simple interpolation. This is why all quality settings perform identically at half resolution.

**Recommendation:**
- **Fast previews**: Use `quality: 0` or `quality: 1` with `halfSize: true` (~370ms)
- **Full-res draft**: Use `quality: 0` with `halfSize: false` for 2.7× speedup
- **Final export**: Use `quality: 3` with `halfSize: false`

## HEIC/HEIF Optimization

The `quality` option also affects HEIC decoding through libheif:

### Quality Impact on HEIC

| Quality | Settings | Time | Speedup | Notes |
|---------|----------|------|---------|-------|
| 0 (Draft) | ignore_transformations, non-strict | 228 ms | **1.15×** | Skip crop/rotation, faster validation |
| 3 (Best) | strict mode, apply transformations | 261 ms | baseline | Full validation and processing |

### HEIC-Specific Optimizations

**Draft mode (quality: 0) enables:**
- `ignore_transformations` - Skips crop, rotation, mirroring (~5-10% faster)
- `strict_decoding = false` - Relaxed validation
- Default codec threading (libde265 multithreading)

**Metadata extraction (much faster):**
```javascript
// Get dimensions without full decode - ~0.2ms vs ~260ms
const decoder = new nImage.ImageDecoder('heic');
const meta = decoder.getMetadata(buffer);
// { width: 3024, height: 4032, ... }
```

**Thumbnail extraction:**
```javascript
// Extract embedded thumbnail - ~5ms vs ~260ms
const thumb = await nImage.thumbnail(buffer, { size: 512, format: 'heic' });
```

### Build Configuration

The vcpkg-built libheif includes:
- ✅ `ENABLE_MULTITHREADING_SUPPORT=ON` - Multi-threading enabled
- ✅ `ENABLE_PARALLEL_TILE_DECODING=ON` - Parallel tile decode
- ⚠️ No hardware acceleration - libde265 is CPU-only

### Why HEIC is Slower Than JPEG

| Factor | Impact |
|--------|--------|
| H.265 codec | 5-10× more compute than JPEG |
| libde265 | Software-only, no GPU/DXVA |
| Typical speed | ~260ms for 12MP (vs ~50ms JPEG) |

**Alternatives for faster HEIC:**
1. Use thumbnails (5ms)
2. Use metadata (0.2ms) + Sharp for scaling
3. Build with FFmpeg backend (experimental, hardware decode)

## Test Assets

Available in `test/assets/`:
- `IMG_2593.CR2` - Canon CR2
- `_4260446.ORF` - Olympus ORF
- `IMG_0092_1.HEIC` - Apple HEIC

Missing samples: NEF, ARW, RAF, AVIF

## Performance Targets

| Operation | Target | Status |
|-----------|--------|--------|
| Format detection | < 1 µs | ✅ ~0.5-0.6 µs |
| RAW decode (20MP) | < 600ms | ✅ ~400-520ms |
| HEIC decode (12MP) | < 150ms | ✅ < 100ms |
| JPEG encode | < 100ms | 🔲 TODO |
| PNG encode | < 150ms | 🔲 TODO |

## Error Codes

| Code | Cause |
|------|-------|
| `UNSUPPORTED_FORMAT` | Unknown/unsupported format |
| `DECODE_FAILED` | Corrupt file or missing codec |
| `ENCODE_FAILED` | Invalid parameters |
| `OUT_OF_MEMORY` | Image too large |
| `MODULE_NOT_LOADED` | Build missing |

## Working with NAPI

- Uses `node-addon-api` (official NAPI C++ wrapper)
- Use `Napi::ObjectWrap<T>` for classes (see `ImageDecoderWrapper`)
- Return Buffers with `Napi::Buffer<uint8_t>::Copy()`
- All `std::string` fields must be converted to `Napi::String`
- Error handling: `Napi::Error::New(env, msg).ThrowAsJavaScriptException()`

---

## Electron Integration

### The MinGW Problem

**CRITICAL**: The precompiled binaries in `dist/` are compiled with MinGW and **WILL CRASH** in Electron on Windows. This is due to an Import Address Table (IAT) bug where MinGW-compiled NAPI addons have empty import tables for `napi_*` functions, causing 0xC0000005 Access Violation when the module loads.

**Solution**: Build with MSVC using `binding.gyp` + node-gyp.

### Required DLLs (Windows)

The following runtime DLLs must be available for nImage to load:

| DLL | Source | Purpose |
|-----|--------|---------|
| `heif.dll` | vcpkg | HEIC/HEIF decoding |
| `raw_r.dll` | vcpkg | LibRaw (RAW formats) |
| `raw.dll` | vcpkg | LibRaw base |
| `libde265.dll` | vcpkg | HEVC decoder |
| `libx265.dll` | vcpkg | H.265 encoder |
| `zlib1.dll` | vcpkg | Compression |
| `jasper.dll` | vcpkg | JPEG-2000 support |
| `jpeg62.dll` | vcpkg | JPEG codec |
| `lcms2-2.dll` | vcpkg | Color management |
| `turbojpeg.dll` | vcpkg | Fast JPEG |

All DLLs are automatically copied to `build/Release/` during the build process.

### Building for Electron

```powershell
# 1. Build native module for Node.js (MSVC)
npm run build

# 2. Rebuild for specific Electron version
npx electron-rebuild -f -w nimage -v <electron-version>

# Example for Electron 41.1.1:
npx electron-rebuild -f -w nimage -v 41.1.1
```

### Using in Electron Renderer

```javascript
// stage.js - Renderer process
const path = require('path');

function initNImage(appBasePath) {
    if (!window.electron_helper) return false;
    
    try {
        // CRITICAL: Use './nImage' not '../nImage' to avoid loading from parent directory
        const nImageBinPath = path.resolve(appBasePath || __dirname, './nImage/build/Release');
        
        // Add DLL folder to PATH before loading
        process.env.PATH = nImageBinPath + ';' + (process.env.PATH || '');
        
        // CRITICAL: Use './nImage' to ensure we load from THIS project, not a parent folder
        nImage = require('./nImage/lib/index.js');
        return nImage?.isLoaded || false;
    } catch (e) {
        console.log('nImage unavailable:', e.message);
        return false;
    }
}

// Call during app startup
const nImageReady = initNImage(g.basePath);
```

**⚠️ CRITICAL: Path Resolution Trap**

Using `../nImage` in an Electron app can cause it to load from a parent directory instead of the project folder:

```
Work_GIT/
├── nImage/                 ← OLD binary (wrong!)
├── BlankTest/              ← Your project
│   ├── js/stage.js
│   └── nImage/             ← Correct binary (ignored!)
```

If `stage.js` uses `path.resolve(__dirname, '../nImage')` when `__dirname` is `Work_GIT/BlankTest`, it resolves to `Work_GIT/nImage` instead of `Work_GIT/BlankTest/nImage`.

**Always use `./nImage`** to ensure the module loads from the current project directory.

### Reliable Binary Management with bin/ Directory

For Electron apps, use a project-level `bin/` directory as a reliable binary source:

**Project structure:**
```
BlankTest/
├── bin/                    # Reliable binary location (commit to git)
│   ├── nimage.node
│   └── *.dll              # Required runtime DLLs
├── nImage/                 # Submodule (builds to build/Release/)
├── scripts/
│   └── copy-native-bin.js # Copy script
└── forge.config.js
```

**1. Update `lib/index.js` loading order:**
```javascript
// lib/index.js - Try project bin first
const path = require('path');

try {
    // 1. Try project-level bin (reliable for Electron)
    // nImage/lib -> nImage/ -> project-root/bin/
    const projectBinPath = path.join(__dirname, '..', '..', 'bin', 'nimage.node');
    nativeBinding = require(projectBinPath);
} catch (e) {
    // 2. Try build directory (development)
    const buildPath = path.join(__dirname, '..', 'build', 'Release', 'nimage.node');
    nativeBinding = require(buildPath);
    // ... fallbacks
}
```

**2. Copy script (`scripts/copy-native-bin.js`):**
```javascript
const fs = require('fs');
const path = require('path');

const sourcePaths = [
    path.join(__dirname, '..', 'nImage', 'build', 'Release', 'nimage.node'),
    path.join(__dirname, '..', 'nImage', 'dist', 'nimage.node'),
];
const targetDir = path.join(__dirname, '..', 'bin');
const targetPath = path.join(targetDir, 'nimage.node');

// Copy binary
for (const sourcePath of sourcePaths) {
    if (fs.existsSync(sourcePath)) {
        fs.copyFileSync(sourcePath, targetPath);
        console.log(`Copied: ${sourcePath}`);
        break;
    }
}

// Copy DLLs on Windows
if (process.platform === 'win32') {
    const dllSourceDir = path.join(__dirname, '..', 'nImage', 'build', 'Release');
    const dllNames = ['heif.dll', 'raw_r.dll', /* ... */];
    for (const dll of dllNames) {
        const src = path.join(dllSourceDir, dll);
        const dst = path.join(targetDir, dll);
        if (fs.existsSync(src)) fs.copyFileSync(src, dst);
    }
}
```

**3. Add npm scripts:**
```json
{
  "scripts": {
    "build:native": "cd nImage && npm run build",
    "copy:native": "node scripts/copy-native-bin.js",
    "rebuild:native": "npm run build:native && npm run copy:native"
  }
}
```

**4. Update `forge.config.js`:**
```javascript
module.exports = {
  packagerConfig: {
    extraResource: [
      "config.json",
      "./bin/"              // Include binaries in package
    ],
  }
};
```

**Binary loading priority (after setup):**
1. `../../bin/nimage.node` ← **Reliable, version-controlled**
2. `../build/Release/nimage.node` ← Development builds
3. `../dist/nimage.node` ← Distribution fallback

This ensures Electron always loads the correct binary regardless of forge's rebuild behavior.

### Graceful Degradation

Always check `nImage.isLoaded` before using native features. The JS wrapper provides:
- `detectFormat()` - Pure JS format detection (always works)
- `getSupportedFormats()` - Returns list of supported formats
- Sharp pipeline fallback for standard formats

### Distribution

When packaging your Electron app:
1. Include `nImage/build/Release/*.dll` (all 10 DLLs)
2. Include `nImage/build/Release/nimage.node`
3. Include `nImage/lib/index.js`

The module will gracefully fail if DLLs are missing, logging which files are absent.
