# nImage

Native image codec for Node.js via NAPI — decode, transform, and encode images.

**High-performance image processing:**
- **RAW formats**: CR2, NEF, ARW, ORF, RAF, DNG, etc. (via libraw)
- **HEIC/HEIF**: Apple's modern image format (via libheif)
- **Everything else**: Sharp handles JPEG, PNG, WebP, TIFF, AVIF

**nImage wraps Sharp with RAW/HEIC superpowers.** Use the Sharp API you know, get RAW/HEIC support automatically.

## Status

**Last Updated**: 2026-04-03

| Component | Status |
|-----------|--------|
| Format Detection | ✅ Complete |
| LibRawDecoder (RAW) | ✅ Working |
| LibHeifDecoder (HEIC) | ✅ Working |
| Sharp Pipeline | 🔲 TODO |
| Pre-compiled Binaries | ✅ In dist/ |

### Benchmark Results

```
Format Detection: ~0.5-0.6 µs per call (1.7-2M ops/sec)
RAW Decode (Olympus ORF 9MB): 3720x2800 in ~400ms
RAW Decode (Canon CR2 22MB): 5208x3476 in ~520ms
HEIC Decode (1.9MB): 2316x3088 in <100ms
Pipeline (RAW→resize→JPEG): TBD
```

## Why nImage?

**Sharp can't decode RAW or HEIC natively.** That's the gap nImage fills.

```javascript
// Sharp-compatible API - works with RAW, HEIC, and everything else
const thumb = await nImage('photo.cr2')  // RAW from Canon
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 80 })
    .toBuffer();

// nImage detects RAW format, decodes via libraw,
// then passes RGB to Sharp for the pipeline
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         nImage Module                            │
│                                                                  │
│  Input              nImage处理          Sharp处理          Output  │
│  ──────            ───────           ─────────         ─────    │
│  RAW ────────────► [Decode]────────► [Transform]────► JPEG    │
│  HEIC ──────────► via libraw    ──► [Resize]───────► PNG     │
│  JPEG ─────────────────────────────► [Crop]───────► WebP     │
│  PNG  ─────────────────────────────► [Rotate]────► AVIF     │
│  WebP ────────────────────────────────────────────────────► (any)│
│  TIFF ────────────────────────────────────────────────────► (any)│
└─────────────────────────────────────────────────────────────────┘

nImage处理: RAW/HEIC decoding via libraw/libheif to RGB/RGBA
Sharp处理: All transformations and encoding (libvips)

nImage presents a Sharp-compatible API. RAW/HEIC are decoded
transparently, then the full Sharp pipeline is available.
```

## Supported Formats

### Input Formats (nImage handles RAW/HEIC natively, delegates rest to Sharp)

| Format | Handler | Status |
|--------|---------|--------|
| Canon CR2/CRW | nImage (libraw) | ✅ Working |
| Nikon NEF | nImage (libraw) | ✅ Working |
| Sony ARW | nImage (libraw) | ✅ Working |
| Olympus ORF | nImage (libraw) | ✅ Working |
| Fujifilm RAF | nImage (libraw) | ✅ Working |
| Panasonic RW2 | nImage (libraw) | ✅ Working |
| Adobe DNG | nImage (libraw) | ✅ Working |
| Pentax PEF | nImage (libraw) | ✅ Working |
| Samsung SRW | nImage (libraw) | ✅ Working |
| Leica RWL | nImage (libraw) | ✅ Working |
| HEIC | nImage (libheif) | ✅ Working |
| HEIF | nImage (libheif) | ✅ Working |
| AVIF | nImage (libheif) | ✅ Working |
| JPEG | Sharp | ✅ Direct |
| PNG | Sharp | ✅ Direct |
| WebP | Sharp | ✅ Direct |
| TIFF | Sharp | ✅ Direct |
| GIF | Sharp | ✅ Direct |

### Output Formats

All output goes through Sharp (libvips):

| Format | Status |
|--------|--------|
| JPEG | ✅ Via Sharp |
| PNG | ✅ Via Sharp |
| WebP | ✅ Via Sharp |
| AVIF | ✅ Via Sharp |

## Installation

### Pre-built Binaries (Recommended)

The module ships with pre-compiled binaries in `dist/`:
- `nimage.node` - Native module
- 51 runtime DLLs (libraw, libheif, and dependencies)

```javascript
const nImage = require('nimage');
```

### Build from Source

#### Windows

```powershell
# Run as Administrator in PowerShell
cd nImage
.\scripts\setup.ps1
```

This script will:
1. Download and install MSYS2 (if not present)
2. Install libraw, libheif, and encoder libraries via pacman
3. Build the native module using MinGW g++
4. Run tests

#### Linux

```bash
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev
npm run build
```

## Build Commands

| Command | Description |
|---------|-------------|
| `.\scripts\setup.ps1` | Full setup: install MSYS2, deps, build, test (Windows) |
| `.\scripts\setup.ps1 -SkipInstall` | Only build (deps already present) |
| `npm run setup:deps` | Only install dependencies, don't build |
| `npm run build` | Build native module |
| `npm run build:debug` | Build with debug symbols |
| `npm test` | Run tests |
| `node test/benchmark.js` | Run performance benchmarks |

## API

### Sharp-Compatible Pipeline (Primary API)

```javascript
const nImage = require('nimage');

// Works with RAW, HEIC, and standard formats - same API
const thumb = await nImage('photo.cr2')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 80 })
    .toBuffer();

const optimized = await nImage('image.heic')
    .resize(1920, 1080, { fit: 'inside' })
    .webp({ quality: 85 })
    .toBuffer();

const converted = await nImage('photo.png')
    .jpeg({ quality: 90 })
    .toBuffer();
```

### Low-Level Decode (when you need raw pixels)

```javascript
const nImage = require('nimage');

const imageBuffer = fs.readFileSync('image.cr2');
const result = nImage.decode(imageBuffer);

console.log(result.width, result.height);  // 6000, 4000
console.log(result.format);               // 'cr2'
console.log(result.channels);            // 3 (RGB)
console.log(result.data);               // Buffer with raw RGB pixel data
```

### Format Detection

```javascript
const result = nImage.detectFormat(buffer);
console.log(result.format);     // 'heic'
console.log(result.confidence); // 0.95
console.log(result.mimeType);  // 'image/heic'
```

## Return Value

`nImage.decode()` returns:

```typescript
{
    width: number;
    height: number;
    bitsPerChannel: number;      // 8 or 16
    channels: number;            // 3=RGB, 4=RGBA
    colorSpace: string;          // 'sRGB', etc.
    format: string;              // 'cr2', 'heic', etc.
    hasAlpha: boolean;
    data: Buffer | null;        // Raw pixel data
    iccProfile: Buffer | null;
    camera: { make, model };
    capture: { dateTime, exposureTime, fNumber, isoSpeed, focalLength };
    raw: { width, height };
    orientation: number;         // EXIF 1-8
}
```

## Performance

Format detection is ~0.5µs regardless of buffer size (reads only magic bytes).

RAW/HEIC decode times:

| File | Size | Dimensions | Decode Time |
|------|------|------------|------------|
| Olympus ORF | 9.4 MB | 3720 x 2800 | ~400 ms |
| Canon CR2 | 21.8 MB | 5208 x 3476 | ~520 ms |
| HEIC | 1.9 MB | 2316 x 3088 | <100 ms |

## Module Structure

```
nImage/
├── src/
│   ├── decoder.h       # Base decoder class
│   ├── decoder.cpp     # LibRawDecoder, LibHeifDecoder
│   └── binding.cpp     # NAPI bindings
├── lib/
│   └── index.js       # JavaScript entry point + Sharp wrapper
├── dist/              # Pre-compiled binaries (tracked in git)
│   ├── nimage.node   # Native module
│   └── *.dll         # 51 runtime DLLs
├── deps/              # Build dependencies (gitignored)
├── scripts/
│   ├── setup.ps1      # Full setup script
│   └── build.js       # Build script (direct g++)
├── test/
│   ├── index.test.js  # Unit tests
│   ├── benchmark.js   # Performance benchmarks
│   └── assets/        # Test images
├── docs/              # Documentation
│   ├── nImage_spec.md # Architecture spec
│   └── nImage_dev_plan.md # Development plan
└── package.json
```

## Roadmap

See [docs/nImage_dev_plan.md](docs/nImage_dev_plan.md) for full development plan.

### Near-term (v2.0)
- [ ] Sharp integration - complete pipeline
- [ ] Standard formats via Sharp (JPEG, PNG, WebP, TIFF, GIF)
- [ ] All outputs via Sharp (JPEG, PNG, WebP, AVIF)

### Future
- [ ] LittleCMS ICC color management
- [ ] Tile-based processing for large images
- [ ] Thumbnail extraction

## License

MIT - David Renelt

## Repository

https://github.com/herrbasan/nImage
