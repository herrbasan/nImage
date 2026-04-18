# nImage API Documentation

**nImage** is a native Node.js module for image processing via N-API. It wraps Sharp with native codec support for RAW formats (CR2, NEF, ARW, ORF, RAF, DNG), HEIC/HEIF/AVIF, and 150+ additional formats via ImageMagick fallback.

## Table of Contents

- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [API Reference](#api-reference)
  - [Pipeline API](#pipeline-api)
  - [Format Detection](#format-detection)
  - [Decode](#decode)
  - [Thumbnail Extraction](#thumbnail-extraction)
  - [Streaming / Tile Decode](#streaming--tile-decode)
  - [Capabilities](#capabilities)
- [Classes](#classes)
  - [NImagePipeline](#nimagepipeline)
  - [ImageDecoder](#imagedecoder)
  - [BufferPool](#bufferpool)
- [Decode Options](#decode-options)
- [Quality Presets](#quality-presets)
- [Supported Formats](#supported-formats)
- [Error Handling](#error-handling)
- [Electron Support](#electron-support)

---

## Quick Start

```javascript
const nImage = require('nimage');

// Pipeline API (Sharp-compatible) - works with any format
const result = await nImage('photo.cr2')
    .resize(1024, 1024, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Low-level decode (raw RGB pixels)
const imageData = nImage.decode(buffer);
console.log(imageData.width, imageData.height, imageData.channels);

// Fast thumbnail extraction
const thumb = await nImage.thumbnail(buffer, { size: 256 });

// Format detection
const info = nImage.detectFormat(buffer);
console.log(info.format, info.mimeType);
```

---

## Core Concepts

### Architecture

nImage extends Sharp with native codec support for formats Sharp cannot handle:

1. **RAW formats** (CR2, NEF, ARW, ORF, etc.) → LibRaw decode → RGB → Sharp pipeline
2. **HEIC/HEIF/AVIF** → LibHeif decode → RGB → Sharp pipeline
3. **Standard formats** (JPEG, PNG, WebP, etc.) → Sharp directly
4. **150+ other formats** (PDF, SVG, PSD, EXR, etc.) → ImageMagick fallback → RGB → Sharp pipeline

### Decoder Priority Chain

```
Input Buffer
     │
     ▼
Format Detection (magic bytes)
     │
     ├─ RAW format?    → LibRawDecoder  → RGB
     ├─ HEIC/AVIF?     → LibHeifDecoder → RGB
     ├─ Standard?      → Sharp directly
     └─ Other format?  → MagickDecoder  → RGB
                                        │
                                        ▼
                              Sharp Transform/Encode
                                        │
                                        ▼
                                   Output Buffer
```

### Module Loading

nImage tries to load the native binary from multiple locations:

1. `../../bin/nimage.node` — project-level bin (reliable for Electron)
2. `../build/Release/nimage.node` — development builds
3. `../prebuilds/nimage.node` — installed package
4. `../dist/nimage.node` — distribution fallback

If no native binary is found, `nImage.isLoaded` will be `false` and native operations will throw helpful errors. `detectFormat()` always works (pure JS).

---

## API Reference

### Pipeline API

The primary API is a Sharp-compatible pipeline that handles all formats transparently.

```javascript
// Create pipeline from file path
const result = await nImage('photo.cr2')
    .resize(1920, 1080, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Create pipeline from Buffer
const result = await nImage(imageBuffer)
    .rotate(90)
    .webp({ quality: 80 })
    .toBuffer();

// Create pipeline from ImageData
const result = await nImage(imageData)
    .grayscale()
    .png()
    .toBuffer();
```

### Format Detection

#### `nImage.detectFormat(buffer)`

Detect image format from magic bytes. Always available (pure JS, no native dependency).

**Parameters:**
- `buffer` (Buffer): Image data (minimum 12 bytes)

**Returns:** `FormatDetection`
```typescript
interface FormatDetection {
  format: string;      // 'cr2', 'heic', 'jpeg', 'png', 'unknown', etc.
  confidence: number;  // 0.0 - 1.0
  mimeType: string;    // 'image/heic', 'image/jpeg', etc.
}
```

**Example:**
```javascript
const info = nImage.detectFormat(buffer);
if (info.format === 'cr2') {
  console.log('Canon RAW file');
}
```

### Decode

#### `nImage.decode(buffer, options?)`

Decode image buffer to raw RGB/RGBA pixel data.

**Parameters:**
- `buffer` (Buffer): Image data
- `formatHint` (string, optional): Format hint (e.g., `'cr2'`, `'heic'`)
- `options` (object, optional): Decode options

**Options:**
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `halfSize` | boolean | `false` | Decode at half resolution (4x faster) |
| `quality` | number | `1` | Quality preset: 0=draft, 1=fast, 2=balanced, 3=best |
| `format` | string | auto | Format hint |

**Returns:** `ImageData`
```typescript
interface ImageData {
  width: number;
  height: number;
  bitsPerChannel: number;     // 8 or 16
  channels: number;           // 3=RGB, 4=RGBA
  colorSpace: string;         // 'sRGB', 'AdobeRGB', etc.
  format: string;             // 'cr2', 'heic', etc.
  hasAlpha: boolean;
  data: Buffer;               // Raw pixel data
  iccProfile: Buffer | null;  // ICC color profile
  camera: { make: string, model: string };
  capture: {
    dateTime: string;
    exposureTime: number;
    fNumber: number;
    isoSpeed: number;
    focalLength: number;
  };
  raw: { width: number, height: number };
  orientation: number;        // EXIF 1-8
}
```

**Example:**
```javascript
const buffer = fs.readFileSync('photo.cr2');

// Basic decode
const image = nImage.decode(buffer);
console.log(`${image.width}x${image.height}, ${image.channels} channels`);

// Fast preview (half resolution, draft quality)
const preview = nImage.decode(buffer, { halfSize: true, quality: 0 });

// With format hint
const heicImage = nImage.decode(buffer, 'heic');
```

#### `nImage.getSupportedFormats()`

Returns array of all supported format names.

**Returns:** `string[]` — 212+ format names

```javascript
const formats = nImage.getSupportedFormats();
console.log(formats.length); // 212
```

### Thumbnail Extraction

#### `nImage.thumbnail(input, options?)`

Extract a thumbnail without full image decode. Uses embedded thumbnails for RAW/HEIC (fast), Sharp resize for standard formats.

**Parameters:**
- `input` (string | Buffer): File path or Buffer
- `options` (object):
  - `size` (number): Max dimension (default: 256)
  - `format` (string): Format hint (optional)

**Returns:** `Promise<ImageData>` — Thumbnail image data

**Example:**
```javascript
// Fast thumbnail from RAW
const thumb = await nImage.thumbnail(buffer, { size: 256 });
console.log(`${thumb.width}x${thumb.height}`);

// Pipeline method
const thumb = await nImage('photo.cr2').thumbnail({ size: 512 });
```

**Performance:**

| Format | Method | Time |
|--------|--------|------|
| RAW (CR2, NEF, etc.) | Native `unpack_thumb()` | ~5-10ms |
| HEIC/HEIF | Native `get_thumbnail()` | ~5ms |
| JPEG, PNG, WebP | Sharp `resize()` | ~10-50ms |
| PDF, PSD, EXR, etc. | MagickDecoder resize | Slow (full decode) |

### Streaming / Tile Decode

#### `nImage.stream(buffer, options?)`

Decode image as tiles for memory-efficient processing of large images.

**Parameters:**
- `buffer` (Buffer): Image data
- `options` (object):
  - `tileSize` (number): Tile size in pixels (default: 2048)
  - `format` (string): Format hint (optional)

**Returns:** `Promise<ImageData[]>` — Array of tile data with position info

**Example:**
```javascript
const tiles = await nImage.stream(buffer, { tileSize: 2048 });
for (const tile of tiles) {
  console.log(`Tile at (${tile.x}, ${tile.y}): ${tile.width}x${tile.height}`);
}
```

### Capabilities

#### `nImage.getCapabilities()`

Returns pre-generated capability data describing all supported formats, decoders, and features.

```javascript
const caps = nImage.getCapabilities();
console.log(caps.decoders.raw.formats);   // RAW format list
console.log(caps.decoders.heic.formats);  // HEIC format list
```

See [CAPABILITIES.md](CAPABILITIES.md) for full documentation.

---

## Classes

### NImagePipeline

Sharp-compatible pipeline with automatic format handling. Created by calling `nImage(input)`.

**Constructor:** `nImage(input)` where input is a path string, Buffer, or ImageData.

#### Transform Methods

All methods return `this` for chaining.

| Method | Parameters | Description |
|--------|------------|-------------|
| `resize(w, h, opts)` | width, height, { fit } | Resize image |
| `crop(w, h)` | width, height | Crop to region |
| `extract(opts)` | { left, top, width, height } | Extract region |
| `rotate(angle, opts)` | angle (0, 90, 180, 270) | Rotate image |
| `flip(opts)` | - | Flip vertically |
| `flop(opts)` | - | Flip horizontally |
| `grayscale(opts)` | - | Convert to grayscale |
| `negate(opts)` | - | Invert colors |
| `normalize(opts)` | - | Normalize contrast |
| `linear(a, b, opts)` | alpha, beta | Apply linear transform |
| `extend(opts)` | { top, bottom, left, right } | Add padding |
| `composite(overlay, opts)` | overlay, options | Composite images |

#### Output Format Methods

All methods return `this` for chaining.

| Method | Parameters | Description |
|--------|------------|-------------|
| `jpeg(opts)` | { quality, preserveExif, stripExif } | Output as JPEG |
| `png(opts)` | { compressionLevel } | Output as PNG |
| `webp(opts)` | { quality, lossless } | Output as WebP |
| `avif(opts)` | { quality } | Output as AVIF |
| `tiff(opts)` | { compression } | Output as TIFF |

#### Execution Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `toBuffer()` | `Promise<Buffer>` | Execute pipeline, return buffer |
| `toFile(path)` | `Promise<this>` | Execute pipeline, write to file |
| `metadata()` | `Promise<object>` | Get metadata (triggers decode) |
| `raw()` | `Promise<Buffer>` | Get raw pixel data |
| `thumbnail(opts)` | `Promise<ImageData>` | Extract thumbnail |
| `stream(opts)` | `Promise<ImageData[]>` | Tile-based decode |

#### Accessors

| Method | Returns | Description |
|--------|---------|-------------|
| `getSharp()` | Sharp instance | Access underlying Sharp instance |
| `getMetadata()` | object \| null | Get native decode metadata (RAW/HEIC only) |

---

### ImageDecoder

Low-level class-based decoder for direct pixel access.

**Constructor:** `new nImage.ImageDecoder(format?)` where format is optional (auto-detect if omitted).

| Method | Returns | Description |
|--------|---------|-------------|
| `decode(buffer)` | ImageData | Decode buffer to RGB |
| `getMetadata(buffer)` | ImageMetadata | Get metadata without full decode |
| `getError()` | string | Get last error message |

**Example:**
```javascript
const decoder = new nImage.ImageDecoder('cr2');
const metadata = decoder.getMetadata(buffer);
console.log(`${metadata.width}x${metadata.height}, ${metadata.camera.make} ${metadata.camera.model}`);

const image = decoder.decode(buffer);
```

---

### BufferPool

Pre-allocated buffer pool for zero-GC batch processing.

**Constructor:** `new nImage.BufferPool(options)` or `nImage.createBufferPool(options)`

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `maxBuffers` | number | 8 | Maximum pooled buffers |

| Method | Returns | Description |
|--------|---------|-------------|
| `acquire(size)` | Buffer | Get a buffer of at least `size` bytes |
| `release(buf)` | void | Return buffer to pool |
| `clear()` | void | Free all pooled buffers |

**Example:**
```javascript
const pool = nImage.createBufferPool({ maxBuffers: 16 });

const files = ['a.cr2', 'b.cr2', 'c.cr2'];
for (const file of files) {
  const buffer = fs.readFileSync(file);
  const image = nImage.decode(buffer);
  const buf = pool.acquire(image.width * image.height * 3);
  // ... process pixels into buf ...
  pool.release(buf);
}
pool.clear();
```

---

## Decode Options

### Options Object

```javascript
nImage.decode(buffer, {
  halfSize: false,   // Half resolution (4x faster)
  quality: 1,        // 0=draft, 1=fast, 2=balanced, 3=best
  format: 'cr2'      // Optional format hint
});
```

### halfSize

When `true`, decodes at half resolution (1/4 pixels, ~4x faster). LibRaw uses simple interpolation instead of complex demosaic algorithms. All quality settings perform identically at half resolution.

### format

Optional format hint. If omitted, format is auto-detected from magic bytes. Useful for formats that share magic bytes (e.g., TIFF-based RAW formats).

---

## Quality Presets

The `quality` option controls the demosaic algorithm and processing parameters. Affects both RAW and HEIC decoding.

### RAW Formats

| Quality | Algorithm | Processing | Use Case |
|---------|-----------|------------|----------|
| 0 (Draft) | Linear interpolation | No camera WB, no highlight recovery | Ultra-fast previews |
| 1 (Fast) | PPG demosaic | Camera WB | Default previews |
| 2 (Balanced) | AHD demosaic | Camera WB, clip highlights | High quality |
| 3 (Best) | AHD+ demosaic | Full processing, highlight reconstruction | Final export |

**Full Resolution (3476x5208):**

| Quality | Time | Speedup |
|---------|------|---------|
| 0 | ~630ms | 2.7x |
| 1 | ~707ms | 2.4x |
| 2 | ~1693ms | 1.0x |
| 3 | ~1719ms | baseline |

**Half Resolution (1738x2604):** All qualities ~370ms (half_size bypasses demosaic choice).

### HEIC/HEIF Formats

| Quality | Settings | Time | Speedup |
|---------|----------|------|---------|
| 0 (Draft) | ignore_transformations, non-strict | ~228ms | 1.15x |
| 3 (Best) | strict mode, apply transformations | ~261ms | baseline |

### Recommendations

- **Fast previews**: `quality: 0` or `quality: 1` with `halfSize: true` (~370ms)
- **Full-res draft**: `quality: 0` with `halfSize: false` (~630ms, 2.7x faster)
- **Final export**: `quality: 3` with `halfSize: false` (maximum quality)

---

## Supported Formats

### RAW (LibRaw) — 22 formats
CR2, CRW, NEF, NRW, ARW, SR2, ORF, RAF, RW2, DNG, PEF, SRW, RWL, MRW, ERF, 3FR, K25, KDC, MEF, MOS, MRAW, RRF, RWZ

### HEIC/HEIF/AVIF (LibHeif) — 3 formats
HEIC, HEIF, AVIF

### Standard (Sharp) — 9 formats
JPEG, PNG, WebP, TIFF, GIF, BMP, JXL, JP2

### Documents (ImageMagick) — 13 formats
PDF, SVG, AI, DOC, DOCX, XLS, XLSX, PPT, PPTX, EPS, XPS, PSD, PSB

### Scientific (ImageMagick) — 20 formats
EXR, HDR, BigTIFF, CIN, DPX, FITS, FLIF, J2K, JP2, MIFF, MPC, PCD, PFM, PICT, PPM, PSP, SGI, TGA, VTF

### Video Stills (ImageMagick) — 12 formats
AVI, MOV, MP4, M4V, MPG, MPEG, WMV, FLV, MKV, MNG, JNG, MPO

### Other (ImageMagick) — 150+ formats
AAI, ART, BLP, BMP2, BMP3, CUR, DIB, DDS, DJVU, EMF, GRAY, ICO, PBM, PGM, PNM, RAS, SUN, WBMP, WMF, XBM, XPM, XWD, and many more.

### Output Encoding (Sharp)
JPEG, PNG, WebP, AVIF, TIFF

---

## Error Handling

### Pipeline Errors

Pipeline errors include the full operation chain for debugging:

```javascript
try {
  await nImage('photo.cr2').resize(999999).jpeg().toBuffer();
} catch (err) {
  console.log(err.message);
  // "nImage pipeline failed (resize → jpeg): maximum image dimension is 100000"
  console.log(err.pipeline);
  // { input, operations, outputFormat, formatDetected }
}
```

### Error Codes

| Code | Cause |
|------|-------|
| `UNSUPPORTED_FORMAT` | Unknown/unsupported format |
| `DECODE_FAILED` | Corrupt file or missing codec |
| `ENCODE_FAILED` | Invalid parameters |
| `OUT_OF_MEMORY` | Image too large |
| `MODULE_NOT_LOADED` | Build missing |

### Module Not Loaded

Always check `nImage.isLoaded` before using native features:

```javascript
if (!nImage.isLoaded) {
  console.error('Native module not loaded:', nImage.loadError);
  // Fallback: detectFormat() still works (pure JS)
}
```

---

## Electron Support

### Building for Electron

```powershell
# Build for Electron (example: v41.1.1)
npx electron-rebuild -f -w nimage -v 41.1.1
```

### Important Notes

- **Use MSVC builds only** — MinGW-compiled binaries crash in Electron due to IAT corruption
- **Delete `build/Release/nimage.node`** before rebuilding to prevent stale binary loading
- **Copy DLLs** from `build/Release/` to `dist/` after rebuild

### Integration Pattern

```javascript
// In your Electron app
const nImage = require('./nImage/lib/index.js');

if (!nImage.isLoaded) {
  console.warn('nImage native module not available');
}
```

See [ELECTRON.md](ELECTRON.md) for detailed Electron integration guide.

---

## License

MIT - David Renelt
