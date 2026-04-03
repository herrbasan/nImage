# nImage

Native image decoding via NAPI for Node.js.

High-performance image decoding using native libraries:
- **libraw**: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
- **libheif**: HEIC/HEIF formats (Apple's modern image format)
- **ICC color profile support** (via LittleCMS)

## Why nImage?

Traditional image decoding in Node.js relies on:
- **ImageMagick CLI**: Slow (process spawn overhead), heavyweight
- **Sharp**: Limited format support (no RAW, limited HEIC without FFmpeg)
- **ffmpeg for images**: Overkill for simple image decode

nImage provides:
- In-process native decoding (no CLI spawn overhead)
- Direct memory access for pixel data
- Native ICC color management
- Support for professional RAW formats
- Modern Apple HEIC/HEIF support

## Supported Formats

### RAW Formats (via libraw)
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

### HEIC/HEIF (via libheif)
- HEIC (High Efficiency Image Format)
- HEIF (High Efficiency Image Format)
- AVIF (AV1 Image Format)

### Standard Formats
- JPEG, PNG, WebP, TIFF, GIF

## Installation

### One-Command Setup (Windows)

```powershell
# Run as Administrator in PowerShell
cd modules/nImage
.\scripts\setup.ps1
```

This script will:
1. Download and install MSYS2 (if not present)
2. Install libraw and libheif packages
3. Copy dependencies to `deps/`
4. Build the native module
5. Run tests

### Prerequisites

- **Node.js 18+**
- **Windows** with PowerShell (run as Administrator)

### If MSYS2 Already Installed

If MSYS2 is already on your system, you can skip installation:

```powershell
.\scripts\setup.ps1 -SkipInstall
```

### Linux

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

## API

### `nImage.decode(buffer, [formatHint])`

Decode an image buffer directly.

```javascript
const nImage = require('nimage');

const imageBuffer = fs.readFileSync('image.cr2');
const result = nImage.decode(imageBuffer);

console.log(result.width, result.height);  // 6000, 4000
console.log(result.format);                // 'cr2'
console.log(result.colorSpace);            // 'Adobe RGB'
console.log(result.data);                   // Buffer with RGBA pixel data
```

### `nImage.detectFormat(buffer)`

Detect image format without full decode.

```javascript
const result = nImage.detectFormat(buffer);
console.log(result.format);     // 'heic'
console.log(result.confidence); // 0.95
console.log(result.mimeType);  // 'image/heic'
```

### `nImage.getSupportedFormats()`

Returns array of supported format names.

```javascript
const formats = nImage.getSupportedFormats();
// ['cr2', 'nef', 'arw', 'heic', 'jpeg', ...]
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
  width: number;           // Decoded image width
  height: number;          // Decoded image height
  bitsPerChannel: number;  // Bits per channel (8 or 16)
  channels: number;        // 3=RGB, 4=RGBA
  colorSpace: string;     // 'sRGB', 'Adobe RGB', etc.
  format: string;         // Original format
  hasAlpha: boolean;       // Alpha channel present
  data: Buffer | null;     // Pixel data (RGBA)
  iccProfile: Buffer | null; // ICC color profile
  camera: {
    make: string;          // Camera manufacturer
    model: string;         // Camera model
  };
  capture: {
    dateTime: string;
    exposureTime: number;  // Seconds
    fNumber: number;       // Aperture
    isoSpeed: number;
    focalLength: number;   // mm
  };
  raw: {
    width: number;        // Sensor width
    height: number;       // Sensor height
  };
  orientation: number;     // EXIF orientation (1-8)
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

## License

MIT

## Author

David Renelt
