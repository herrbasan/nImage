# nImage

Native image codec for Node.js via NAPI — decode, transform, and encode images.

**High-performance image processing using native libraries:**
- **libraw**: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
- **libheif**: HEIC/HEIF formats (Apple's modern image format)
- **Sharp**: Transformations (resize, crop, rotate) and output encoding

## Status

**Last Updated**: 2026-04-03

| Component | Status |
|-----------|--------|
| Format Detection | ✅ Complete |
| LibRawDecoder (RAW) | ✅ Working |
| LibHeifDecoder (HEIC) | ✅ Working |
| JpegEncoder | 🔲 TODO |
| PngEncoder | 🔲 TODO |
| WebPEncoder | 🔲 TODO |
| Sharp Integration | 🔲 TODO |
| Pre-compiled Binaries | ✅ In dist/ |

### Benchmark Results

```
Format Detection: ~0.5-0.6 µs per call (1.7-2M ops/sec)
RAW Decode (Olympus ORF 9MB): 3720x2800 in ~400ms
RAW Decode (Canon CR2 22MB): 5208x3476 in ~520ms
HEIC Decode (1.9MB): 2316x3088 in <100ms
```

## Why nImage?

**Traditional approaches have weaknesses:**
- **ImageMagick CLI**: Slow (process spawn overhead), heavyweight, security issues
- **Sharp alone**: Can't decode RAW, limited HEIC support
- **FFmpeg for images**: Overkill for simple operations

**nImage provides:**
- In-process native decoding (no CLI spawn overhead)
- RAW and HEIC support that FFmpeg/sharp can't match
- Sharp integration for high-performance transformations
- Native encoders for JPEG, PNG, WebP output
- Single module for the complete image pipeline

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         nImage Module                            │
│                                                                  │
│  Input              Decode           Transform        Encode      │
│  ──────            ──────           ─────────        ─────       │
│  RAW ─────────────► RGB/RGBA ──────► Sharp ◄─────── PNG        │
│  HEIC ────────────►              ──►        ───────► JPEG       │
│  JPEG ◄─────────────────────────────────────────────► WebP     │
│  PNG                                                ──► AVIF     │
└─────────────────────────────────────────────────────────────────┘

Legend: ──► native    ──► Sharp (libvips)
```

## Supported Formats

### Input Formats (Decoding)

| Format | Library | Status |
|--------|---------|--------|
| Canon CR2/CRW | libraw | ✅ Working |
| Nikon NEF | libraw | ✅ Working |
| Sony ARW | libraw | ✅ Working |
| Olympus ORF | libraw | ✅ Working |
| Fujifilm RAF | libraw | ✅ Working |
| Panasonic RW2 | libraw | ✅ Working |
| Adobe DNG | libraw | ✅ Working |
| Pentax PEF | libraw | ✅ Working |
| Samsung SRW | libraw | ✅ Working |
| Leica RWL | libraw | ✅ Working |
| HEIC | libheif | ✅ Working |
| HEIF | libheif | ✅ Working |
| AVIF | libheif+aom | ✅ Working |
| JPEG | libjpeg | 🔲 TODO |
| PNG | libpng | 🔲 TODO |
| WebP | libwebp | 🔲 TODO |
| TIFF | libtiff | 🔲 TODO |

### Output Formats (Encoding)

| Format | Library | Status |
|--------|---------|--------|
| JPEG | libjpeg | 🔲 TODO |
| PNG | libpng | 🔲 TODO |
| WebP | libwebp | 🔲 TODO |
| AVIF | Sharp/libaom | 🔲 TODO |

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

### Detection (always available)

```javascript
const result = nImage.detectFormat(buffer);
console.log(result.format);     // 'heic'
console.log(result.confidence); // 0.95
console.log(result.mimeType);  // 'image/heic'
```

### Decoding

```javascript
const nImage = require('nimage');

const imageBuffer = fs.readFileSync('image.cr2');
const result = nImage.decode(imageBuffer);

console.log(result.width, result.height);  // 6000, 4000
console.log(result.format);               // 'cr2'
console.log(result.channels);            // 3 (RGB)
console.log(result.data);               // Buffer with raw RGB pixel data
```

### Encoding (TODO)

```javascript
const jpegBuffer = nImage.encode(
    rgbBuffer,      // Raw RGB/RGBA pixel data
    width,          // Image width
    height,         // Image height
    channels,       // 3=RGB, 4=RGBA
    'jpeg',         // Output format
    { quality: 85 } // Options
);
```

### Sharp Pipeline (TODO)

```javascript
const nImage = require('nimage');
const sharp = require('sharp');

// Decode RAW to raw RGB
const imageData = nImage.decode(fs.readFileSync('photo.cr2'));

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
```

### Classes

```javascript
// Decoder for batch operations
const decoder = new nImage.ImageDecoder('cr2');
const result = decoder.decode(imageBuffer);

// Encoder for output (TODO)
const encoder = new nImage.ImageEncoder('jpeg');
const jpegBuffer = encoder.encode(rgbBuffer, width, height, channels, options);
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
│   ├── encoder.h       # Base encoder class
│   ├── encoder.cpp     # JpegEncoder, PngEncoder, WebPEncoder
│   └── binding.cpp     # NAPI bindings
├── lib/
│   └── index.js       # JavaScript entry point
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
- [ ] JPEG encoder
- [ ] PNG encoder
- [ ] WebP encoder
- [ ] Sharp integration for transforms
- [ ] Standard format decoders (JPEG, PNG, WebP, TIFF)

### Future
- [ ] AVIF encoder
- [ ] LittleCMS ICC color management
- [ ] Tile-based processing for large images
- [ ] Thumbnail extraction

## License

MIT - David Renelt

## Repository

https://github.com/herrbasan/nImage
