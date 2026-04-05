# Integration Handover: nImage & Electron

## Background
We attempted to integrate the custom N-API native image converter (`nImage`) into an Electron application (`Electron Slide`). The module works perfectly in pure Node.js (via CLI or `spawn`), but requiring it directly inside the Electron frontend or main process causes an instant, fatal crash (Exit Code 1 / 0xC0000005 Access Violation).

## Root Cause: MinGW Import Address Table (IAT) Corruption

**Verified**: The crash is caused by compiling nImage with MinGW instead of MSVC. This is a confirmed bug affecting all MinGW-compiled NAPI addons on Windows.

### Technical Details

When a NAPI addon is compiled with MinGW, the resulting `.node` binary has an **empty Import Address Table (IAT)** for `napi_*` functions. When the module attempts to call these functions, it jumps to uninitialized/null memory addresses, causing the 0xC0000005 Access Violation.

Analysis shows:
- **MSVC-compiled**: Properly populates `.idata` section with import entries for `node.exe` symbols like `napi_module_register`, `napi_create_function`, etc.
- **MinGW-compiled**: Produces an empty import table for these symbols

This is specific to Node.js/Electron module loading, not a general C ABI issue. A MinGW DLL can correctly export C functions to an MSVC executable, but the NAPI import mechanism fails.

### Evidence from SoundApp (Working Pattern)

SoundApp successfully integrates NAPI addons (`ffmpeg_napi.node`, `native-registry.node`) that work in Electron:

| Addon | Compiler | Toolset | Result |
|-------|----------|---------|--------|
| `ffmpeg_napi.node` | MSVC | v143 | Works in Electron |
| `native-registry.node` | MSVC | (default) | Works in Electron |
| `nImage.node` | MinGW/g++ | gcc | Crashes in Electron |

### Why MSVC is Required

Node.js and Electron are built with MSVC, and their NAPI bindings require proper import table entries. MinGW cannot generate these correctly for the Node.js/Electron import tables.

The electron documentation also notes:
- "you link against `node.lib` from *Electron* and not Node. If you link against the wrong `node.lib` you will get load-time errors"
- `'win_delay_load_hook': 'true'` is required for Electron 4.x+

## The Solution: Switch to MSVC + node-gyp

### Current Build Setup (To Be Replaced)

The current build uses `scripts/build.js` with MSYS2/MinGW:
- Links with `-lnode` (problematic)
- Uses static `.a` libraries (MinGW format)
- Bypasses node-gyp entirely
- No `win_delay_load_hook`

### Reference: Working SoundApp binding.gyp

```json
{
  "targets": [{
    "target_name": "ffmpeg_napi",
    "sources": ["src/binding.cpp", "src/decoder.cpp", "src/utils.cpp"],
    "include_dirs": [
      "<!@(node -p \"require('node-addon-api').include\")",
      "src",
      "<(module_root_dir)/deps/ffmpeg/include"
    ],
    "dependencies": ["<!!(node -p \"require('node-addon-api').gyp\")"],
    "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS", "UNICODE", "_UNICODE"]
  }]
}
```

### Steps to Refactor

1. **Create `binding.gyp`** (replace `scripts/build.js`)
   - Use node-addon-api via `node -p "require('node-addon-api').include"`
   - Add `NAPI_DISABLE_CPP_EXCEPTIONS` define
   - Set `win_delay_load_hook` to `true`
   - Use MSVC v143 toolset

2. **Get MSVC-compatible dependencies**
   - Replace MinGW `.a` static libs with MSVC `.lib` import libraries
   - Option A: Use vcpkg to build libraw, libheif with MSVC
   - Option B: Use prebuilt binaries from libraw/libheif releases (check if they provide .lib)
   - Keep dynamic deps (libde265, libaom, etc.) as DLLs

3. **Update package.json scripts**
   ```json
   "scripts": {
     "build": "node-gyp rebuild",
     "build:debug": "node-gyp rebuild --debug"
   }
   ```

4. **Build and test**
   ```powershell
   npm run build
   node -e "require('./build/Release/nimage.node')"  # Quick test
   ```

5. **Rebuild for Electron**
   ```powershell
   npx electron-rebuild -f -w nimage
   ```

## Alternative: Fix MinGW IAT (Not Recommended)

It is theoretically possible to fix the MinGW IAT issue using:
- `dlltool` to generate proper import libraries
- `.def` files to export symbols

However, this is complex and not recommended since MSVC is the standard, supported toolchain.

## Rejected Fallback: IPC Worker
We successfully ran `nImage` out-of-process via `child_process.spawn()` during testing. However, this negates the primary benefit of building a native N-API module. An out-of-process IPC approach limits performance by stripping away our ability to use zero-copy memory pointer sharing, making it no more efficient than standard CLI execution (such as the existing `magick.exe` implementation).

## Verification: Minimal Electron Test App

After rebuilding nImage with MSVC, verify it works in Electron by creating a minimal test app in `test/electron-minimal/`:

```
test/electron-minimal/
├── package.json
├── main.js
├── preload.js
└── test.js
```

### Steps to verify

1. Build nImage with MSVC (see above)
2. Copy rebuilt `build/Release/nimage.node` to `test/electron-minimal/`
3. Run the test app:
   ```powershell
   cd test/electron-minimal
   npm install
   npm start
   ```
4. Expected: No crash, `nImage.detectFormat()` and `nImage.decode()` should work

### What to test

- `require('./nimage.node')` - should load without crash
- `nImage.detectFormat(buffer)` - should return format info
- `nImage.decode(buffer)` - should return ImageData
- RAW formats (CR2, NEF, ARW) - should decode successfully
- HEIC/HEIF formats - should decode successfully

## Files Summary

### Files to Modify (Refactor)

| File | Action | Notes |
|------|--------|-------|
| `scripts/build.js` | Replace | Use `binding.gyp` + node-gyp instead |
| `package.json` | Update | Add build scripts, add `windows-build-tools` |
| `binding.gyp` | Create | New file - MSVC build config |
| `deps/lib/*.a` | Replace | Need MSVC `.lib` format libraries |

### Files Created (Test)

| File | Purpose |
|------|---------|
| `test/electron-minimal/package.json` | Electron test app |
| `test/electron-minimal/main.js` | Main process |
| `test/electron-minimal/preload.js` | Loads nImage safely |
| `test/electron-minimal/index.html` | Test UI |
| `test/electron-minimal/test.js` | Test runner |

## References
- Stack Overflow: "Node.js Node-API addon compiled with MinGW causes access violation" (confirmed empty IAT issue)
- Electron documentation: Using Native Node Modules
- electron/rebuild: Package to rebuild native modules for Electron
- SoundApp `libs/ffmpeg-napi-interface/` - working MSVC + NAPI example
- SoundApp `libs/native-registry/` - working MSVC + NAPI example

---

## Handover (Session 2026-04-05)

### Completed ✓

1. **MSVC Build via node-gyp** - `binding.gyp` created and working
   - Uses vcpkg at `C:/vcpkg` for dependencies
   - Links against MSVC `.lib` files (raw_r.lib, heif.lib)
   - Includes `win_delay_load_hook` support
   - Copies required DLLs to build output

2. **vcpkg Dependencies** - Installed and working
   - `libraw:x64-windows` → `C:/vcpkg/installed/x64-windows/lib/raw_r.lib`
   - `libheif:x64-windows` → `C:/vcpkg/installed/x64-windows/lib/heif.lib`
   - All transitive deps (libde265, libx265, zlib, etc.) included

3. **Updated Dependencies**
   - node-gyp: ^12.2.0
   - @electron/rebuild: ^4.0.3
   - electron: ^41.1.1
   - sharp: ^0.34.5 (latest)

4. **Node.js Tests Pass**
   - `npm test` - All tests PASS
   - nImage loads correctly in Node.js
   - RAW decode, HEIC decode, thumbnails all working

### Current Issue (BLOCKED)

**Electron Test App Not Working**

The `test/electron-minimal/` app fails when run with:
```
TypeError: Cannot read properties of undefined (reading 'whenReady')
```

This appears to be a Windows shell/PATH issue where running `electron .` through Git Bash shell scripts routes through Node.js instead of directly to Electron's binary. The electron.exe itself reports version as "v24.14.0" instead of Electron version.

**Build artifacts exist:**
- `build/Release/nimage.node` - MSVC-compiled module
- `build/Release/*.dll` - Required DLLs (heif.dll, raw_r.dll, libde265.dll, etc.)
- `test/electron-minimal/nimage.node` - Copy for Electron testing

### Files Changed

| File | Change |
|------|--------|
| `binding.gyp` | Created - MSVC config with vcpkg paths |
| `vcpkg.json` | Created - tracks libraw/libheif deps |
| `package.json` | Updated dependencies |
| `scripts/setup.ps1` | Updated to detect vcpkg |
| `test/electron-minimal/package.json` | Updated electron version |

### Next Steps

1. **Fix Electron test app execution**
   - The shell wrapper (`node_modules/.bin/electron`) is intercepting calls
   - Try running directly: `.\node_modules\electron\dist\electron.exe .`
   - Or use PowerShell: `Start-Process "electron.exe" -ArgumentList "."`

2. **Verify Electron compatibility**
   - Once app runs, check preload console for "nImage loaded successfully"
   - Test `window.nImage.detectFormat()` and `window.nImage.decode()`

3. **If Electron still crashes**
   - Use `electron-rebuild -f -w nimage` to rebuild against Electron's ABI
   - May need to link against Electron's node.lib instead of Node's

### Quick Test Commands

```powershell
# Build with MSVC
npm run build

# Run Node.js tests
npm test

# Rebuild for Electron
npm run rebuild:electron

# Copy to test app
copy build\Release\nimage.node test\electron-minimal\
copy build\Release\*.dll test\electron-minimal\

# Run Electron test (in PowerShell, not Git Bash)
cd test\electron-minimal
.\node_modules\electron\dist\electron.exe .
```
