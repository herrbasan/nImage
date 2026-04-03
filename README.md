# nImage

Native image decoding via NAPI for Node.js.

High-performance image decoding using native libraries:
- **libraw**: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
- **libheif**: HEIC/HEIF formats (Apple's modern image format) - ✅ Working
- **ICC color profile support** (via LittleCMS) - *future*

## Status

**Last Updated**: 2026-04-03

| Component | Status |
|-----------|--------|
| Format Detection | ✅ Complete |
| LibRawDecoder (RAW) | ✅ Working |
| LibHeifDecoder (HEIC) | ✅ Working |
| ICC Color Management | ⬜ Future |
| Pre-compiled Binaries | ✅ In dist/ |

### Benchmark Results

```
Format Detection: ~0.5-0.6 µs per call (1.7-2M ops/sec)
RAW Decode (Olympus ORF 9MB): 3720x2800 in ~400ms
RAW Decode (Canon CR2 22MB): 5208x3476 in ~520ms
HEIC Decode (1.9MB): 2316x3088 in <100ms
```

## Why nImage?

Traditional image decoding in Node.js relies on:
- **ImageMagick CLI**: Slow (process spawn overhead), heavyweight
- **Sharp**: Limited format support (no RAW, limited HEIC without FFmpeg)
- **ffmpeg for images**: Overkill for simple image decode

nImage provides:
- In-process native decoding (no CLI spawn overhead)
- Direct memory access for pixel data
- Native ICC color management (future)
- Support for professional RAW formats
- Modern Apple HEIC/HEIF support (future)

## Supported Formats

### RAW Formats (via libraw) - ✅ WORKING
- Canon: CR2, CRW
- Nikon: NEF
- Sony: ARW
- Olympus: ORF
- Fujifilm: RAF
- Panasonic: RW2
- Adobe: DNG
- Pentax: PEF
- Samsung: SRW
- Leica: RWL

### HEIC/HEIF (via libheif) - ⏳ PENDING
- HEIC (High Efficiency Image Format)
- HEIF (High Efficiency Image Format)
- AVIF (AV1 Image Format)

### Standard Formats
- JPEG, PNG, WebP, TIFF, GIF (via format detection, decoding via external libraries)

## Installation

### Pre-built Binaries (Recommended)

The module ships with pre-compiled binaries in `dist/`:
- `nimage.node` - Native module
- 51 runtime DLLs (libraw, libheif, and dependencies)

Simply require the module:
```javascript
const nImage = require('nimage');
```

### Build from Source

#### Windows

```powershell
# Run as Administrator in PowerShell
cd modules/nImage
.\scripts\setup.ps1
```

This script will:
1. Download and install MSYS2 (if not present)
2. Install libraw and libheif packages via pacman
3. Copy dependencies to `deps/`
4. Build the native module using MinGW g++
5. Run tests

#### If MSYS2 Already Installed

```powershell
.\scripts\setup.ps1 -SkipInstall
```

#### Linux

```bash
sudo apt install libraw-dev libheif-dev
mkdir -p deps/include deps/lib deps/bin
cp /usr/include/*.h deps/include/ 2>/dev/null || true
cp /usr/lib/x86_64-linux-gnu/libraw* deps/lib/
cp /usr/lib/x86_64-linux-gnu/libheif* deps/lib/
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

### `nImage.decode(buffer, [formatHint])`

Decode an image buffer directly.

```javascript
const nImage = require('nimage');

const imageBuffer = fs.readFileSync('image.cr2');
const result = nImage.decode(imageBuffer);

console.log(result.width, result.height);  // 6000, 4000
console.log(result.format);                // 'cr2'
console.log(result.colorSpace);            // 'sRGB'
console.log(result.data);                  // Buffer with RGB pixel data
```

### `nImage.detectFormat(buffer)`

Detect image format without full decode.

```javascript
const result = nImage.detectFormat(buffer);
console.log(result.format);     // 'heic'
console.log(result.confidence);  // 0.95
console.log(result.mimeType);    // 'image/heic'
```

### `nImage.getSupportedFormats()`

Returns array of supported format names.

```javascript
const formats = nImage.getSupportedFormats();
// ['cr2', 'nef', 'arw', 'heic', 'jpeg', ...]
```

### `nImage.ImageDecoder`

Class-based API for multiple operations.

```javascript
const decoder = new nImage.ImageDecoder('cr2');
const result = decoder.decode(imageBuffer);
console.log(result.width, result.height);
```

### `nImage.isLoaded`

Check if native module is loaded.

```javascript
if (nImage.isLoaded) {
  // Native decoding available
} else {
  // Only JS fallback available (format detection works)
}
```

## Return Value

`nImage.decode()` returns an object:

```typescript
{
  width: number;              // Decoded image width
  height: number;              // Decoded image height
  bitsPerChannel: number;      // Bits per channel (8 or 16)
  channels: number;           // 3=RGB, 4=RGBA
  colorSpace: string;         // 'sRGB', 'Adobe RGB', etc.
  format: string;             // Original format
  hasAlpha: boolean;          // Alpha channel present
  data: Buffer | null;         // Pixel data (RGB/RGBA)
  camera: {
    make: string;             // Camera manufacturer
    model: string;            // Camera model
  };
  capture: {
    exposureTime: number;     // Seconds
    fNumber: number;          // Aperture
    isoSpeed: number;
    focalLength: number;      // mm
  };
  raw: {
    width: number;           // Sensor width
    height: number;          // Sensor height
  };
  orientation: number;        // EXIF orientation (1-8)
}
```

## Error Handling

```javascript
const nImage = require('nimage');

try {
  const result = nImage.decode(buffer);
  // Success
} catch (err) {
  if (!nImage.isLoaded) {
    console.error('Native module not built - run npm run build');
  } else {
    console.error('Decode failed:', err.message);
  }
}
```

## Module Structure

```
modules/nImage/
├── src/
│   ├── decoder.h       # Base decoder class and structures
│   ├── decoder.cpp     # Decoder implementations (LibRaw)
│   └── binding.cpp    # NAPI bindings
├── lib/
│   └── index.js       # JavaScript entry point
├── dist/              # Pre-compiled binaries (tracked in git)
│   ├── nimage.node    # Native module
│   └── *.dll         # Runtime dependencies
├── deps/              # Build dependencies (gitignored)
├── scripts/
│   ├── setup.ps1      # Full setup script
│   └── build.js      # Build script (uses direct g++)
├── test/
│   ├── index.test.js  # Unit tests
│   ├── benchmark.js   # Performance benchmarks
│   └── convert-to-jpeg.js  # Conversion tool
└── package.json
```

## Performance

Format detection is extremely fast (~0.5µs regardless of buffer size) because it only reads magic bytes.

RAW decoding speed depends on file size and camera:

| File | Size | Dimensions | Decode Time |
|------|------|------------|------------|
| Olympus ORF | 9.4 MB | 3720 x 2800 | ~400 ms |
| Canon CR2 | 21.8 MB | 5208 x 3476 | ~520 ms |

## License

MIT

## Author

David Renelt
