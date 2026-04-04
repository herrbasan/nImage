# nImage Static Compilation for Electron Integration

**Goal**: Build a self-contained `nimage.node` with static-linked C++ dependencies for RAW/HEIC, eliminating DLL collision with Electron's pre-loaded graphics libraries.

**Incremental Approach**:
1. **Phase 1-4**: Static link RAW + HEIC, CLI fallback for edge cases (this document)
2. **Phase 5+**: (Future) Static link full ImageMagick if needed

**Root Problem**: Electron v41 pre-loads graphics DLLs (vulkan, libjpeg, libpng, etc.). Windows `LoadLibrary` links nImage to Electron's in-memory copies instead of nImage's shipped DLLs → ABI mismatch → crash.

**Solution Architecture**:
```
dist/
├── nimage.node    # Static-linked (LibRaw + LibHeif only)
│                   # No external DLL dependencies
│
└── magick.exe    # Existing ImageMagick CLI for edge cases
                    # Handles: TGA, PSD, EXR, and other rare formats
                    # Invoked via child_process.spawn() - no DLL collision
```

**Format Routing**:
| Format Type | Handler | Notes |
|-------------|---------|-------|
| RAW (CR2, NEF, ARW, ORF, RAF, etc.) | LibRaw (static) | Native |
| HEIC, HEIF, AVIF | LibHeif (static) | Native |
| JPEG, PNG, WebP, GIF, TIFF, BMP | Sharp (JS) | Already works |
| TGA, PSD, EXR, and other rare formats | magick.exe CLI | Fallback |

**Key Realization**: ImageMagick is ONLY needed for edge cases. No need to static-link ImageMagick now - use CLI fallback first.

**Success Criteria**: `require('nimage')` works inside Electron main/renderer process without crash.

---

## Phase 1: Dependency Audit

### 1.1 Check Existing libraw
**Task**: Verify libraw.a is truly static (not an import lib)

```bash
file deps/lib/libraw.a
# True static: "current ar archive" - no DLL imports
# Import lib: "current ar archive" with "DLL Name" entries

# Alternative check:
objdump -p deps/lib/libraw.a | grep "DLL Name"
# No output = truly static
```

**Current**: `deps/lib/libraw.a` exists (1.6MB) - likely static, verify

### 1.2 Check Existing libheif
**Task**: Check if static libheif exists

```bash
file deps/lib/libheif.a
# If missing, need to build from source

objdump -p deps/lib/libheif.a | grep "DLL Name"
# If has DLL Name entries, it's an import lib (not static)
```

**Current**: Only `deps/lib/libheif.dll.a` exists (274KB) - **import lib, not static**

### 1.3 Verify magick.exe CLI
**Task**: Confirm ImageMagick CLI is available for fallback

```bash
# Check if magick.exe exists in deps/bin or system
where magick.exe
# or
ls deps/bin/magick.exe
```

**Expected**: magick.exe should exist from previous setup

---

## Phase 2: Build Static libheif

### 2.1 Install libheif Dependencies (if needed)
**Task**: Build any codecs libheif needs statically

libheif can use: x265, aom (AV1), dav1d (AV1), xvid

**Check**: Is libheif already built with static deps?
```bash
objdump -p deps/lib/libheif.a | grep "DLL Name"
```

### 2.2 Build libheif from Source
**Task**: Compile libheif with static configuration

```bash
cd deps/src/libheif
rm -rf build
mkdir build && cd build
cmake .. \
  -DENABLE_SHARED=OFF \
  -DENABLE_STATIC=ON \
  -DENABLE_X265=OFF \
  -DENABLE_AOM=ON \
  -DENABLE_DAV1D=ON \
  -DCMAKE_INSTALL_PREFIX=$(pwd)/../install \
  -G "MinGW Makefiles"
mingw32-make -j4
mingw32-make install
```

**Expected output**: `deps/lib/libheif.a`

### 2.3 Verify Static Linking
**Task**: Confirm libheif.a has no dynamic deps

```bash
objdump -p deps/lib/libheif.a | grep "DLL Name"
# Should have no matches (truly static)
```

---

## Phase 3: Modify Build System

### 3.1 Update build.js
**Task**: Modify `scripts/build.js` for static linking

**Changes**:

1. Add `-static` flags:
   ```javascript
   const cxxFlags = [
     '-std=c++17',
     '-static',
     '-static-libgcc',
     '-static-libstdc++',
     '-shared',
     '-Wall',
     '-O2',
     '-DNDEBUG',
   ];
   ```

2. Link against `.a` files directly:
   ```javascript
   const libs = [
     '-l:libraw.a',
     '-l:libheif.a',
     '-lnode',
     '-luser32',
     '-lgdi32',
   ];
   ```

3. Remove DLL copy step (no external DLLs)

4. Link order: source files first, then libraries

**Output**: Updated `scripts/build.js`

### 3.2 Update Code for CLI Fallback
**Task**: Modify decoder logic to use CLI fallback instead of compiled MagickDecoder

**Changes in `decoder.cpp` / `lib/index.js`**:
- Remove `#include <MagickWand/MagickWand.h>` from C++ (no longer needed)
- Remove `MagickDecoder` class compilation
- In JS layer, when format is rare (TGA, PSD, EXR, etc.):
  ```javascript
  // CLI fallback for edge cases
  const result = spawnSync('magick.exe', [inputPath, '-o', outputPath]);
  ```

**MagickDecoder path becomes**: Not compiled in, only used via CLI

### 3.3 Update dist Packaging
**Task**: Ensure magick.exe is included in dist

```javascript
// In package script or build.js
fs.copyFileSync(
  path.join(MODULE_ROOT, 'deps', 'bin', 'magick.exe'),
  path.join(distDir, 'magick.exe')
);
```

### 3.3 Verify Static Linking
**Task**: Confirm nimage.node has no dynamic DLL deps

```bash
ldd build/Release/nimage.node
# Should only show: ntdll.dll, kernel32.dll, node.exe
```

---

## Phase 4: Electron Integration Testing

### 4.1 Basic Electron Test
**Task**: Test nImage loads in Electron without crash

```javascript
// In Electron main process
const nimage = require('./dist/nimage.node');
console.log('nImage loaded:', Object.keys(nimage));
```

### 4.2 Format Handling Test
**Task**: Test each format path works

| Format | Handler | Test |
|--------|---------|------|
| CR2, NEF, ARW | LibRaw | Should work |
| HEIC, HEIF | LibHeif | Should work |
| JPEG, PNG | Sharp | Already works |
| TGA, PSD | magick.exe | CLI fallback |

### 4.3 Iteration Loop
**Task**: Fix any remaining issues

**Common issues**:
- Missing static symbol → add to linking
- Symbol collision → reorder or use `strip`

---

## Phase 5: Finalize

### 5.1 Update Documentation
- Update `docs/electron_issue.md` with solution
- Document new architecture in `AGENTS.md`

### 5.2 Package for Distribution
```
dist/
├── nimage.node    # Static-linked native module (no external DLLs)
├── magick.exe    # ImageMagick CLI for edge cases
└── sharp/        # Sharp (via npm install)
```

### 5.3 Optional: Full ImageMagick Static (Future)
**When**: If CLI fallback is too slow for production use

**Task**: Static link entire ImageMagick dependency tree

**Note**: This requires rebuilding all ~30 transitive deps as static. Skip unless CLI performance is unacceptable.

---

## Future Enhancement: Full ImageMagick Static Link

If Phase 1-5 proves CLI fallback too slow for edge cases, return here:

1. Build ImageMagick from source with `--enable-static --disable-shared`
2. Ensure all deps (freetype, fontconfig, libpng, libjpeg, etc.) are also static
3. Update build.js to link against `libMagickCore-7.Q16HDRI.a`
4. Remove CLI fallback (native MagickDecoder)

**Effort**: 5-8 hours additional

---

## Risk Factors

| Risk | Impact | Mitigation |
|------|--------|------------|
| libheif static build complex | Medium | Build with minimal codec deps first |
| ImageMagick CLI path untrusted | Low | Already working, just integrate |
| Edge case format not handled | Low | Add to CLI fallback |

---

## Time Estimate

| Phase | Effort |
|-------|--------|
| Phase 1: Dependency Audit | 15 min |
| Phase 2: Build static libheif | 1-2 hrs |
| Phase 3: Build system + CLI fallback | 1-2 hrs |
| Phase 4: Electron Integration Testing | 1 hr |
| **Phase 1-4 Subtotal** | **3-5 hrs** |
| Phase 5: Finalize | 30 min |
| **Total** | **3.5-5.5 hrs** | |

---

## Status

| Phase | Status |
|-------|--------|
| Phase 1: Dependency Audit | ✅ DONE |
| Phase 2: Build Static libheif | ✅ DONE |
| Phase 3: Build System + CLI Fallback | ✅ DONE |
| Phase 4: Electron Integration Testing | ✅ DONE |
| Phase 5: Finalize | ✅ DONE |
| Future: Full ImageMagick Static | ⬜ OPTIONAL |
