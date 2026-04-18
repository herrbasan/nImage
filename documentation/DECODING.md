# Decoding Guide

Complete guide to image decoding with nImage.

## Table of Contents

- [Basic Decoding](#basic-decoding)
- [RAW Format Decoding](#raw-format-decoding)
- [HEIC/HEIF Decoding](#heicheif-decoding)
- [Quality Presets](#quality-presets)
- [Half-Size Decoding](#half-size-decoding)
- [Metadata Extraction](#metadata-extraction)
- [Thumbnail Extraction](#thumbnail-extraction)
- [Streaming / Tile Decode](#streaming--tile-decode)
- [Document Formats](#document-formats)
- [Error Handling](#error-handling)

---

## Basic Decoding

### Decode Any Format

```javascript
const nImage = require('nimage');
const fs = require('fs');

const buffer = fs.readFileSync('photo.cr2');
const image = nImage.decode(buffer);

console.log(image.width);        // 5208
console.log(image.height);       // 3476
console.log(image.channels);     // 3 (RGB)
console.log(image.bitsPerChannel); // 8
console.log(image.format);       // 'cr2'
console.log(image.colorSpace);   // 'sRGB'
console.log(image.hasAlpha);     // false
```

### Decode with Format Hint

```javascript
// When auto-detection might fail, provide a hint
const image = nImage.decode(buffer, 'heic');
```

### Decode with Options

```javascript
const image = nImage.decode(buffer, {
  halfSize: true,    // Half resolution (4x faster)
  quality: 0,        // Draft quality
  format: 'cr2'      // Optional format hint
});
```

### Pipeline Decode (Recommended for Transforms)

```javascript
// Pipeline API handles format detection automatically
const result = await nImage('photo.cr2')
    .resize(1920, 1080, { fit: 'inside' })
    .jpeg({ quality: 85 })
    .toBuffer();
```

---

## RAW Format Decoding

nImage uses LibRaw for all RAW camera formats.

### Supported RAW Formats

| Format | Camera | Extension |
|--------|--------|-----------|
| CR2/CRW | Canon | .cr2, .crw |
| NEF/NRW | Nikon | .nef, .nrw |
| ARW/SR2 | Sony | .arw, .sr2 |
| ORF | Olympus | .orf |
| RAF | Fujifilm | .raf |
| RW2 | Panasonic | .rw2 |
| DNG | Adobe | .dng |
| PEF | Pentax | .pef |
| SRW | Samsung | .srw |
| RWL/MRAW | Leica | .rwl |
| MRW | Minolta | .mrw |
| ERF | Epson | .erf |
| 3FR | Hasselblad | .3fr |
| K25/KDC | Kodak | .k25, .kdc |
| MEF | Mamiya | .mef |
| MOS | Leaf | .mos |
| RRF | Canon (v2) | .rrf |
| RWZ | Rawzor | .rwz |

### RAW-Specific Output

RAW decode includes camera metadata:

```javascript
const image = nImage.decode(fs.readFileSync('photo.cr2'));

console.log(image.camera);
// { make: 'Canon', model: 'Canon EOS 5D Mark IV' }

console.log(image.capture);
// { dateTime: '2024:01:15 14:30:00', exposureTime: 0.008, fNumber: 2.8, isoSpeed: 400, focalLength: 85.0 }

console.log(image.raw);
// { width: 6720, height: 4480 }  // Full RAW sensor dimensions

console.log(image.orientation);
// 1 (normal) - EXIF orientation value

console.log(image.iccProfile);
// Buffer with ICC color profile data
```

### RAW Decode Flow

```
RAW Buffer
     │
     ▼
LibRaw open_buffer()
     │
     ▼
unpack() — Read compressed RAW data
     │
     ▼
dcraw_process() — Demosaic + color conversion
     │  ┌──────────────────────────────┐
     │  │ quality=0: Linear (fastest)    │
     │  │ quality=1: PPG (fast)          │
     │  │ quality=2: AHD (balanced)      │
     │  │ quality=3: AHD+ (best)         │
     │  └──────────────────────────────┘
     │
     ▼
dcraw_make_mem_image() — Get RGB pixel data
     │
     ▼
ImageData { data, width, height, channels: 3, ... }
```

---

## HEIC/HEIF Decoding

nImage uses LibHeif for HEIC/HEIF/AVIF formats.

### Basic HEIC Decode

```javascript
const image = nImage.decode(fs.readFileSync('IMG_0092.HEIC'));
console.log(`${image.width}x${image.height}`); // 3024x4032
console.log(image.channels); // 3 or 4 (depending on alpha)
console.log(image.hasAlpha); // true if image has transparency
```

### HEIC with Quality Options

```javascript
// Fast decode (skip transformations)
const fast = nImage.decode(buffer, { quality: 0 });

// High quality (strict validation)
const best = nImage.decode(buffer, { quality: 3 });
```

### HEIC vs JPEG Performance

| Metric | HEIC | JPEG |
|--------|------|------|
| Codec | H.265 (libde265) | libjpeg |
| Decode (12MP) | ~260ms | ~50ms |
| Codec complexity | 5-10x JPEG | baseline |
| Hardware decode | Not available | Not available |

### Faster HEIC Alternatives

1. **Thumbnails** (~5ms): Use embedded thumbnails
2. **Metadata only** (~0.2ms): Get dimensions without decode
3. **Quality 0** (~228ms): Skip transformations, relaxed validation

---

## Quality Presets

The `quality` option (0-3) controls decode speed vs quality tradeoff.

### RAW Quality Settings

```javascript
// Draft: Linear interpolation, no camera WB
nImage.decode(buffer, { quality: 0 });

// Fast: PPG demosaic, camera WB
nImage.decode(buffer, { quality: 1 });

// Balanced: AHD demosaic, camera WB, clip highlights
nImage.decode(buffer, { quality: 2 });

// Best: AHD+ demosaic, full processing, highlight reconstruction
nImage.decode(buffer, { quality: 3 });
```

### HEIC Quality Settings

```javascript
// Draft: ignore_transformations, non-strict validation
nImage.decode(buffer, { quality: 0 });

// Fast: apply transformations, non-strict
nImage.decode(buffer, { quality: 1 });

// Balanced/Best: strict validation, apply transformations
nImage.decode(buffer, { quality: 2 });
nImage.decode(buffer, { quality: 3 });
```

### Performance Comparison

**RAW Full Resolution (3476x5208):**

| Quality | Time | Speedup | Use Case |
|---------|------|---------|----------|
| 0 | 630ms | 2.7x | Fast previews |
| 1 | 707ms | 2.4x | Default previews |
| 2 | 1693ms | 1.0x | High quality |
| 3 | 1719ms | baseline | Final export |

**RAW Half Resolution (1738x2604):** All qualities ~370ms.

**HEIC (12MP):**

| Quality | Time | Speedup |
|---------|------|---------|
| 0 | 228ms | 1.15x |
| 3 | 261ms | baseline |

---

## Half-Size Decoding

Set `halfSize: true` for 4x faster decode at half resolution.

```javascript
// Full resolution: 5208x3476, ~520ms
const full = nImage.decode(buffer);

// Half resolution: 2604x1738, ~370ms
const half = nImage.decode(buffer, { halfSize: true });
```

When `halfSize` is set, LibRaw skips complex demosaic algorithms and uses simple interpolation. This means `quality` has minimal effect at half resolution.

### Recommendations

| Scenario | Options | Time |
|----------|---------|------|
| Grid/gallery preview | `{ halfSize: true, quality: 0 }` | ~370ms |
| Preview with details | `{ halfSize: true, quality: 1 }` | ~370ms |
| Fast full-res | `{ halfSize: false, quality: 0 }` | ~630ms |
| Final export | `{ halfSize: false, quality: 3 }` | ~1719ms |

---

## Metadata Extraction

Get image metadata without full decode (much faster).

### Using ImageDecoder Class

```javascript
const decoder = new nImage.ImageDecoder('cr2');
const metadata = decoder.getMetadata(buffer);

console.log(metadata);
// {
//   format: 'cr2',
//   width: 5208,
//   height: 3476,
//   rawWidth: 5280,
//   rawHeight: 3528,
//   hasAlpha: false,
//   bitsPerSample: 14,
//   orientation: 1,
//   fileSize: 21877760,
//   camera: { make: 'Canon', model: 'Canon EOS 5D Mark IV' },
//   capture: {
//     exposureTime: 0.008,
//     fNumber: 2.8,
//     isoSpeed: 400,
//     focalLength: 85.0,
//     lensModel: 'EF85mm f/1.8 USM'
//   },
//   colorSpace: 'sRGB'
// }
```

### Using Pipeline API

```javascript
const meta = await nImage('photo.cr2').metadata();
console.log(meta.width, meta.height);
```

### Performance

| Method | Time | Use Case |
|--------|------|----------|
| `decoder.getMetadata()` | ~1-2ms | Dimensions, camera info |
| `nImage.decode()` | ~400-1700ms | Full pixel data |
| `decoder.getMetadata()` (HEIC) | ~0.2ms | Very fast |

---

## Thumbnail Extraction

Extract embedded thumbnails without full decode.

### Standalone Function

```javascript
const thumb = await nImage.thumbnail(buffer, { size: 256 });
console.log(`${thumb.width}x${thumb.height}`);
```

### Pipeline Method

```javascript
const thumb = await nImage('photo.cr2').thumbnail({ size: 512 });
```

### Native Binding (Fastest, No Dependencies)

```javascript
// Direct native call — no Sharp needed
const thumb = nImage.thumbnail(buffer, { size: 256 });
```

### How It Works

```
Format Detection
     │
     ├─ RAW?     → Native unpack_thumb() → ~5-10ms
     ├─ HEIC?    → Native get_thumbnail() → ~5ms
     ├─ Standard? → Sharp resize() → ~10-50ms
     └─ Other?   → MagickDecoder → Slow (full decode)
```

### Performance (12MP HEIC)

| Method | Time | Output |
|--------|------|--------|
| `thumbnail()` | ~5ms | 240x320 embedded |
| `decode()` | ~260ms | 3024x4032 full |

---

## Streaming / Tile Decode

Memory-efficient decode for large images using tiles.

```javascript
const tiles = await nImage.stream(buffer, { tileSize: 2048 });

for (const tile of tiles) {
  console.log(`Tile (${tile.x}, ${tile.y}): ${tile.width}x${tile.height}`);
  // Process tile.data (RGB pixel data)
}
```

### Pipeline Method

```javascript
const tiles = await nImage('large.tiff').stream({ tileSize: 1024 });
```

### Use Cases

- Large image processing without loading entire image into memory
- Progress rendering of large RAW files
- Memory-constrained environments

---

## Document Formats

nImage can rasterize documents to images via ImageMagick fallback.

```javascript
// PDF to JPEG
const result = await nImage('document.pdf')
    .resize(1920, 1080)
    .jpeg({ quality: 85 })
    .toBuffer();

// SVG to PNG
const result = await nImage('logo.svg')
    .png()
    .toBuffer();

// PSD to WebP
const result = await nImage('design.psd')
    .resize(800, 600)
    .webp({ quality: 80 })
    .toBuffer();
```

### Supported Documents

PDF, SVG, AI, DOC, DOCX, XLS, XLSX, PPT, PPTX, EPS, XPS, PSD, PSB

### Performance Notes

Document formats are slower than standard image formats because they require ImageMagick process spawn. Typical times:
- Simple SVG: ~200-500ms
- PDF page: ~500ms-2s
- Complex PSD: ~1-3s

---

## Error Handling

### Decode Errors

```javascript
try {
  const image = nImage.decode(buffer);
} catch (err) {
  if (err.message.includes('UNSUPPORTED_FORMAT')) {
    // Format not recognized
  } else if (err.message.includes('DECODE_FAILED')) {
    // File corrupt or codec missing
  } else if (err.message.includes('MODULE_NOT_LOADED')) {
    // Need to build native module
  }
}
```

### Pipeline Errors

```javascript
try {
  await nImage('photo.cr2').resize(-1).jpeg().toBuffer();
} catch (err) {
  console.log(err.message);
  // "nImage pipeline failed (resize → jpeg): ..."
  console.log(err.pipeline);
  // { input, operations, outputFormat, formatDetected }
}
```

### Thumbnail Fallback

Thumbnail extraction tries multiple approaches:

1. Native embedded thumbnail (fastest)
2. Native embedded thumbnail at larger size
3. Full decode + Sharp resize (fallback, slowest)

If all fail, an error is thrown with the format name.
