# Capabilities Reference

Guide to querying nImage capabilities at runtime.

## Table of Contents

- [Quick Reference](#quick-reference)
- [Static vs Dynamic Capabilities](#static-vs-dynamic-capabilities)
- [Decoders](#decoders)
- [Supported Formats](#supported-formats)
- [Performance Characteristics](#performance-characteristics)
- [Runtime Checks](#runtime-checks)

---

## Quick Reference

```javascript
const nImage = require('nimage');

// Get all capabilities (from static JSON)
const caps = nImage.getCapabilities();
console.log(caps.decoders.raw.formats);

// Check module state
console.log(nImage.isLoaded);  // boolean - native module loaded?
console.log(nImage.hasSharp);  // boolean - Sharp available?

// Get format lists
const formats = nImage.getSupportedFormats();
console.log(formats.length); // 212

// Format-specific lists
console.log(nImage.RAW_FORMATS);           // ['cr2', 'nef', ...]
console.log(nImage.HEIC_FORMATS);          // ['heic', 'heif', 'avif']
console.log(nImage.IMAGEMAGICK_FORMATS);   // ['psd', 'pdf', ...]
```

---

## Static vs Dynamic Capabilities

### Static Capabilities (Fast)

Pre-generated JSON files, no native module call required:

```javascript
const caps = nImage.getCapabilities();

// Or import directly
const capabilities = require('nimage/lib/capabilities/index.json');
```

| Method | Speed | Use Case |
|--------|-------|----------|
| Direct JSON import | Fastest | Build-time decisions |
| `getCapabilities()` | Fast | Runtime reference |
| `getSupportedFormats()` | Moderate | Dynamic format lists |

### Dynamic Checks

```javascript
// Is the native module loaded?
if (!nImage.isLoaded) {
  // Only detectFormat() works (pure JS)
  // All decode operations will throw
}

// Is Sharp available?
if (!nImage.hasSharp) {
  // Pipeline API won't work
  // But native decode/thumbnail still works for RAW/HEIC
}

// Version info
console.log(nImage.version); // { major: 0, minor: 1, patch: 0 }
```

---

## Decoders

nImage uses a priority chain of decoders:

### LibRawDecoder

Handles RAW camera formats via libraw.

| Property | Value |
|----------|-------|
| Formats | CR2, NEF, ARW, ORF, RAF, RW2, DNG, PEF, SRW, RWL, CRW, MRW, NRW, ERF, 3FR, K25, KDC, MEF, MOS, MRAW, RRF, SR2, RWZ |
| Output | RGB (3 channels, 8-bit) |
| Metadata | Camera make/model, EXIF, lens, exposure |
| Thumbnails | Native `unpack_thumb()` (fast) |
| Features | Demosaic algorithms (quality 0-3), half-size decode |

### LibHeifDecoder

Handles HEIC/HEIF/AVIF formats via libheif.

| Property | Value |
|----------|-------|
| Formats | HEIC, HEIF, AVIF |
| Output | RGB or RGBA (3-4 channels, 8-bit) |
| Metadata | Dimensions, orientation, color profile |
| Thumbnails | Native `get_thumbnail()` (fast) |
| Features | Transform handling, strict/relaxed validation |
| Threading | Multi-threading enabled, parallel tile decode |

### Sharp (Standard)

Handles standard image formats directly via libvips.

| Property | Value |
|----------|-------|
| Formats | JPEG, PNG, WebP, TIFF, GIF, BMP, JXL, JP2 |
| Output | Any Sharp-supported format |
| Features | Full Sharp API (resize, crop, rotate, etc.) |

### MagickDecoder (Fallback)

Handles 150+ additional formats via ImageMagick CLI.

| Property | Value |
|----------|-------|
| Formats | PDF, SVG, PSD, AI, EXR, HDR, DOCX, XLSX, PPTX, and 150+ more |
| Output | PNG (intermediate) → Sharp |
| Speed | Slow (full process spawn) |
| Note | Uses `magick.exe` CLI to avoid DLL collisions with Electron |

---

## Supported Formats

### By Category

```javascript
const caps = nImage.getCapabilities();

// RAW formats (LibRaw)
caps.decoders.raw.formats;
// ['cr2', 'nef', 'arw', 'orf', 'raf', 'rw2', 'dng', ...]

// HEIC formats (LibHeif)
caps.decoders.heic.formats;
// ['heic', 'heif', 'avif']

// Standard formats (Sharp)
caps.decoders.sharp.formats;
// ['jpeg', 'png', 'webp', 'tiff', 'gif', 'bmp', 'jxl', 'jp2']

// ImageMagick fallback formats
caps.decoders.magick.formats;
// ['psd', 'pdf', 'svg', 'exr', 'hdr', ...] (150+)

// Output encoding formats
caps.encoders;
// ['jpeg', 'png', 'webp', 'avif', 'tiff']
```

### Format Detection

Magic byte detection coverage:

| Format Family | Detection Method | Confidence |
|---------------|-----------------|------------|
| JPEG | `FF D8 FF` | 0.95 |
| PNG | `89 50 4E 47` | 0.95 |
| TIFF/RAW | `II` or `MM` + magic number | 0.90-0.95 |
| HEIC/HEIF | `ftyp` box + brand | 0.95 |
| AVIF | `ftyp` box + `avif` | 0.95 |
| WebP | `RIFF` + `WEBP` | 0.95 |
| PSD | `8BPS` | 0.95 |
| PDF | `%PDF-` | 0.95 |
| EXR | `76 2F 31 01` | 0.95 |
| HDR | `#?RADIANCE` | 0.95 |
| GIF | `GIF` | 0.95 |

---

## Performance Characteristics

### Decode Performance

| Format | Size | Dimensions | Time | Throughput |
|--------|------|------------|------|------------|
| Canon CR2 | 22 MB | 5208x3476 | ~520ms | ~62 MP/s |
| Olympus ORF | 9 MB | 3720x2800 | ~400ms | ~26 MP/s |
| HEIC | 1.9 MB | 2316x3088 | <100ms | ~72 MP/s |
| JPEG (Sharp) | varies | varies | ~50ms | fast |
| PDF (Magick) | varies | varies | 1-5s | slow |

### Thumbnail Performance

| Format | Method | Time | Notes |
|--------|--------|------|-------|
| RAW | Native `unpack_thumb()` | ~5-10ms | Embedded thumbnail |
| HEIC/HEIF | Native `get_thumbnail()` | ~5ms | Embedded thumbnail |
| JPEG/PNG/WebP | Sharp `resize()` | ~10-50ms | Fast resize |
| PDF/PSD/EXR | MagickDecoder | Slow | Full decode + resize |

### Format Detection

~0.5-0.6 µs per call (1.7-2M ops/sec) — reads only first 12 bytes.

---

## Runtime Checks

### Check Decoder Availability

```javascript
const nImage = require('nimage');

// Native module (LibRaw + LibHeif)
if (nImage.isLoaded) {
  // RAW and HEIC decoding available
  const image = nImage.decode(rawBuffer);
}

// Sharp (transforms + standard formats)
if (nImage.hasSharp) {
  // Pipeline API available
  const result = await nImage('photo.jpg').resize(256, 256).jpeg().toBuffer();
}
```

### Check Format Support

```javascript
const formats = nImage.getSupportedFormats();

function isSupported(format) {
  return formats.includes(format.toLowerCase());
}

function getDecoder(format) {
  if (nImage.RAW_FORMATS.includes(format)) return 'libraw';
  if (nImage.HEIC_FORMATS.includes(format)) return 'libheif';
  if (['jpeg', 'png', 'webp', 'tiff', 'gif', 'bmp'].includes(format)) return 'sharp';
  if (nImage.IMAGEMAGICK_FORMATS.includes(format)) return 'magick';
  return null;
}
```

### Capabilities JSON Structure

```javascript
{
  version: { major: 0, minor: 1, patch: 0 },
  decoders: {
    raw: {
      library: 'libraw',
      formats: ['cr2', 'nef', 'arw', ...],
      features: ['halfSize', 'qualityPresets', 'thumbnails', 'metadata', 'streaming']
    },
    heic: {
      library: 'libheif',
      formats: ['heic', 'heif', 'avif'],
      features: ['thumbnails', 'metadata', 'alphaChannel'],
      threading: true
    },
    sharp: {
      library: 'sharp/libvips',
      formats: ['jpeg', 'png', 'webp', 'tiff', 'gif', 'bmp', 'jxl'],
      features: ['resize', 'crop', 'rotate', 'flip', 'flop', 'grayscale', 'composite']
    },
    magick: {
      library: 'imagemagick',
      formats: ['psd', 'pdf', 'svg', ...],
      features: ['150+ formats'],
      note: 'CLI fallback, slower'
    }
  },
  encoders: ['jpeg', 'png', 'webp', 'avif', 'tiff'],
  detection: {
    method: 'magic-bytes',
    minBytes: 12,
    speed: '~0.5µs'
  }
}
```

---

## Regenerate Static Files

If the native module is updated, regenerate capability files:

```bash
npm run generate-capabilities
```

This updates `lib/capabilities/` with current decoder information.

---

## Static File Structure

```
lib/capabilities/
└── index.json        # All capabilities in one file
```
