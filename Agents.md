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
├── dist/                   # Pre-compiled Windows binaries (51 DLLs + nimage.node)
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
HEIC, HEIF, AVIF, JPEG, PNG, TIFF, WEBP, GIF
```

## Decoders

### LibRawDecoder
- RAW formats: CR2, NEF, ARW, ORF, RAF, RW2, DNG, PEFR, SRW, RWL
- Uses `open_buffer()` for in-memory decoding
- `dcraw_process()` for demosaicing
- Extracts full EXIF metadata

### LibHeifDecoder
- HEIC, HEIF, AVIF formats
- `heif_context_read_from_memory_without_copy()`
- Outputs RGB or RGBA depending on alpha

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

## Development Phases (per dev plan)

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ DONE | Foundation: NAPI bindings, base decoder, format detection |
| 2 | ✅ DONE | LibRaw integration for RAW formats |
| 3 | ✅ DONE | LibHeif integration for HEIC/HEIF |
| 4 | 🔲 TODO | Standard format decoders (JPEG, PNG, WebP, TIFF) |
| 5 | 🔲 TODO | Encoder foundation (base class, factory) |
| 6 | 🔲 TODO | JPEG encoder |
| 7 | 🔲 TODO | PNG encoder |
| 8 | 🔲 TODO | WebP encoder |
| 9 | 🔲 TODO | AVIF encoder (via Sharp initially) |
| 10 | 🔲 TODO | Sharp integration for transforms |
| 11 | ⬜ FUTURE | LittleCMS color management |
| 12 | ⬜ FUTURE | Polish: tiling, streaming, thumbnails |

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

// Encoding (TODO)
nImage.encode(rgbBuffer, width, height, channels, format, options) → Buffer
encoder = new nImage.ImageEncoder('jpeg')
encoder.encode(rgbBuffer, width, height, channels, options) → Buffer
```

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
