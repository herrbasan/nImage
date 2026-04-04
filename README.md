# nImage

Native image codec for Node.js via NAPI — decode, transform, and encode images.

**High-performance image processing:**
- **RAW formats**: CR2, NEF, ARW, ORF, RAF, DNG, etc. (via libraw)
- **HEIC/HEIF**: Apple's modern image format (via libheif)
- **All other formats**: 150+ formats via ImageMagick fallback (PDF, SVG, AI, DOCX, XLSX, PPTX, EXR, HDR, etc.)

**nImage wraps Sharp with RAW/HEIC superpowers.** Use the Sharp API you know, get RAW/HEIC support automatically plus fallback for 150+ obscure formats.

## Status

**Last Updated**: 2026-04-04

| Component | Status |
|-----------|--------|
| Format Detection | ✅ Complete |
| LibRawDecoder (RAW) | ✅ Working |
| LibHeifDecoder (HEIC) | ✅ Working |
| MagickDecoder (150+ formats) | ✅ Working |
| Sharp Pipeline | ✅ Working |
| Pre-compiled Binaries | ✅ In dist/ |

### Benchmark Results

```
Format Detection: ~0.5-0.6 µs per call (1.7-2M ops/sec)
RAW Decode (Olympus ORF 9MB): 3720x2800 in ~400ms
RAW Decode (Canon CR2 22MB): 5208x3476 in ~520ms
HEIC Decode (1.9MB): 2316x3088 in <100ms
Pipeline (RAW→resize→JPEG): ~450-600ms
```

## Why nImage?

**Sharp can't decode RAW or HEIC natively.** That's the gap nImage fills. Plus, nImage handles 150+ formats that neither Sharp nor any other library would handle natively.

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
┌─────────────────────────────────────────────────────────────────────────────┐
│                              nImage Module                                   │
│                                                                             │
│  Input                  Specialized          Sharp处理          Output      │
│  ──────                 ───────────         ─────────         ─────        │
│  RAW ──────────────► [LibRawDecoder]────► [Transform]────► JPEG           │
│  HEIC/AVIF ─────────► [LibHeifDecoder]───► [Resize]───────► PNG           │
│  JPEG/PNG/WebP ──────► [Sharp]───────────► [Crop]───────► WebP           │
│  PDF/SVG/DOCX ───────► [MagickDecoder]───► [Rotate]────► AVIF           │
│  EXR/HDR/CIN ────────► (ImageMagick)─────► [Any Op]────► (any)          │
│  150+ formats ───────► (fallback)────────► ─────────────► ────────────────►│
└─────────────────────────────────────────────────────────────────────────────┘

MagickDecoder is the catch-all fallback for any format not handled by LibRaw or LibHeif.
nImage处理: RAW/HEIC decoding via libraw/libheif to RGB/RGBA
Sharp处理: All transformations and encoding (libvips)
ImageMagick: Handles PDF, SVG, documents, scientific formats, and 150+ other formats
```

**Decoder Priority Chain:**
1. **LibRawDecoder** → RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.)
2. **LibHeifDecoder** → HEIC, HEIF, AVIF
3. **MagickDecoder** → Everything else (PDF, SVG, AI, DOCX, XLSX, PPTX, EXR, HDR, DPX, FITS, video stills, etc.)

## Supported Formats

### RAW Formats (LibRaw) - 22 formats

| Format | Status |
|--------|--------|
| Canon CR2/CRW | ✅ Working |
| Nikon NEF/NRW | ✅ Working |
| Sony ARW/SR2 | ✅ Working |
| Olympus ORF | ✅ Working |
| Fujifilm RAF | ✅ Working |
| Panasonic RW2 | ✅ Working |
| Adobe DNG | ✅ Working |
| Pentax PEF/PEFR | ✅ Working |
| Samsung SRW | ✅ Working |
| Leica RWL/MRAW | ✅ Working |
| Minolta MRW | ✅ Working |
| Epson ERF | ✅ Working |
| Hasselblad 3FR | ✅ Working |
| Kodak K25/KDC | ✅ Working |
| Mamiya MEF | ✅ Working |
| Leaf MOS | ✅ Working |
| Canon RRF | ✅ Working |
| Rawzor RWZ | ✅ Working |
| Gremlin GRB | ✅ Working |

### HEIC/AVIF (LibHeif) - 3 formats

| Format | Status |
|--------|--------|
| HEIC | ✅ Working |
| HEIF | ✅ Working |
| AVIF | ✅ Working |

### Standard Formats (Sharp) - 9 formats

| Format | Status |
|--------|--------|
| JPEG | ✅ Direct |
| PNG | ✅ Direct |
| WebP | ✅ Direct |
| TIFF | ✅ Direct |
| GIF | ✅ Direct |
| BMP | ✅ Direct |
| JXL (JPEG XL) | ✅ Direct |
| JP2 (JPEG 2000) | ✅ Direct |

### Document Formats (ImageMagick) - 13 formats

| Format | Status | Notes |
|--------|--------|-------|
| PDF | ✅ Working | Rasterized to image |
| SVG | ✅ Working | Rasterized to image |
| AI | ✅ Working | Adobe Illustrator |
| DOC | ✅ Working | MS Word (legacy) |
| DOCX | ✅ Working | MS Word (Office Open XML) |
| XLS | ✅ Working | MS Excel (legacy) |
| XLSX | ✅ Working | MS Excel (Office Open XML) |
| PPT | ✅ Working | MS PowerPoint (legacy) |
| PPTX | ✅ Working | MS PowerPoint (Office Open XML) |
| EPS | ✅ Working | Encapsulated PostScript |
| XPS | ✅ Working | XPS Document |
| PSD | ✅ Working | Photoshop Document |
| PSB | ✅ Working | Photoshop Large Document |

### Scientific Formats (ImageMagick) - 20 formats

| Format | Status | Notes |
|--------|--------|-------|
| EXR | ✅ Working | OpenEXR (HDR) |
| HDR | ✅ Working | Radiance HDR |
| DPX | ✅ Working | SMPTE 268M |
| FITS | ✅ Working | Flexible Image Transport System |
| FLIF | ✅ Working | Free Lossless Image Format |
| CIN | ✅ Working | Kodak Cineon |
| JP2/J2K | ✅ Working | JPEG 2000 |
| MIFF | ✅ Working | Magick Image File Format |
| MPC | ✅ Working | Magick Persistent Cache |
| PCD | ✅ Working | Photo CD |
| PFM | ✅ Working | Portable Float Map |
| PICT | ✅ Working | Apple PICT |
| PPM | ✅ Working | Portable Pixel Map |
| PSP | ✅ Working | Paint Shop Pro |
| SGI | ✅ Working | Silicon Graphics RGB |
| TGA | ✅ Working | Truevision TGA |
| VTF | ✅ Working | Nintendo VTF |
| BigTIFF | ✅ Working | TIFF >4GB |

### Video Stills (ImageMagick) - 12 formats

| Format | Status | Notes |
|--------|--------|-------|
| AVI | ✅ Working | Microsoft AVI |
| MOV | ✅ Working | QuickTime |
| MP4/M4V | ✅ Working | MPEG-4 |
| MPG/MPEG | ✅ Working | MPEG-1/2 |
| WMV | ✅ Working | Windows Media Video |
| FLV | ✅ Working | Flash Video |
| MKV | ✅ Working | Matroska |
| MNG | ✅ Working | Multiple-image Network Graphics |
| JNG | ✅ Working | JPEG Network Graphics |
| MPO | ✅ Working | MPO stereo image |
| DCM | ✅ Working | DICOM medical imaging |

### Other Formats (ImageMagick) - 150+ formats

nImage supports 212 formats total via the fallback chain. Additional formats include:

**Raster/Image:** AAI, ART, BLP, BMP2, BMP3, CUR, DIB, DDS, DJVU, EMF, GRAY, GRAYA, ICB, ICC, ICO, MAT, MATTE, MONO, PAL, PALM, PBM, PCDS, PDB, PCX, DCX, PFA, PFB, PGM, PICON, PIX, PJPEG, PLASMA, PNG8, PNG24, PNG32, PNM, RAS, RGB, RGBA, RGB565, RGBA555, RLE, SFW, SUN, TILE, WBMP, WMF, WPG, XBM, XPM, XWD, and more.

**Video:** ANIM, FLI, FLC, and other animation formats.

**Vector/Document:** SVGZ, DOT, XPS-compatible formats.

**Internal/Special:** CLIP, FONT, FRACTAL, G, G3, G4, GRADIENT, GRBO, H, HALD, HCL, HISTOGRAM, HRZ, HTML, HTM, INFO, INLINE, IPL, ISOBRL, ISOBRL6, JBG, JNX, JPE, JPM, JPS, JPX, K, KVEC, LABEL, LINUX, MAC, MAP, MSL, MTV, MVG, NULLIMG, ORA, OTB, POCKETMOD, PS, PS2, PS3, PTIF, PWP, RGF, RLA, RPM, RSLE, SHTML, STREAM, TEXT, TIM, TM2, TTC, TTF, TXT, UBRL, UBRL6, UIL, UYVY, VDA, VIFF, VIPS, WDP, WING, X, XC, XCF, XV, YUV, YUVA, and more.

## Installation

### Pre-built Binaries (Recommended)

The module ships with pre-compiled binaries in `dist/`:
- `nimage.node` - Native module
- 116 runtime DLLs (libraw, libheif, ImageMagick, and dependencies)

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
2. Install libraw, libheif, ImageMagick, and encoder libraries via pacman
3. Build the native module using MinGW g++
4. Run tests

#### Linux

```bash
sudo apt install libraw-dev libheif-dev libjpeg-dev libpng-dev libwebp-dev libtiff-dev libmagick-dev
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

// Works with RAW, HEIC, PDF, and standard formats - same API
const thumb = await nImage('photo.cr2')  // RAW from Canon
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 80 })
    .toBuffer();

// Convert PDF page to image
const pdfThumb = await nImage('document.pdf')
    .resize(256, 256, { fit: 'cover' })
    .jpeg({ quality: 85 })
    .toBuffer();

// Optimize HEIC
const optimized = await nImage('image.heic')
    .resize(1920, 1080, { fit: 'inside' })
    .webp({ quality: 85 })
    .toBuffer();

// Convert any format
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

### Get Supported Formats

```javascript
const formats = nImage.getSupportedFormats();
console.log(formats.length); // 212
console.log(formats);        // Array of all supported format names
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
│   ├── decoder.h       # Base decoder class, ImageFormat enum
│   ├── decoder.cpp     # LibRawDecoder, LibHeifDecoder, MagickDecoder
│   └── binding.cpp     # NAPI bindings
├── lib/
│   └── index.js        # JavaScript entry point + Sharp wrapper
├── dist/               # Pre-compiled binaries (tracked in git)
│   ├── nimage.node    # Native module
│   └── *.dll          # 116 runtime DLLs
├── deps/               # Build dependencies (gitignored)
├── scripts/
│   ├── setup.ps1       # Full setup script
│   └── build.js        # Build script (direct g++)
├── test/
│   ├── index.test.js   # Unit tests
│   ├── benchmark.js    # Performance benchmarks
│   └── assets/         # Test images
├── docs/               # Documentation
│   ├── nImage_spec.md  # Architecture spec
│   └── nImage_dev_plan.md # Development plan
└── package.json
```

## Roadmap

See [docs/nImage_dev_plan.md](docs/nImage_dev_plan.md) for full development plan.

### Completed
- [x] Format Detection
- [x] LibRawDecoder (RAW formats)
- [x] LibHeifDecoder (HEIC/HEIF/AVIF)
- [x] ImageMagick fallback (150+ formats)
- [x] Sharp pipeline integration (transforms + encoding)

### Near-term
- [ ] Multi-page PDF support
- [ ] Document ingest utilities

### Future
- [ ] LittleCMS ICC color management
- [ ] Tile-based processing for large images
- [ ] Thumbnail extraction
- [ ] OCR integration

**Note:** All encoding (JPEG, PNG, WebP, AVIF, TIFF) is handled by Sharp. No separate encoders needed.

## License

MIT - David Renelt

## Repository

https://github.com/herrbasan/nImage
