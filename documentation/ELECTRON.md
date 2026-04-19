# Electron Integration Guide

Using nImage as a submodule in Electron applications.

## Table of Contents

- [Quick Start](#quick-start)
- [Building for Electron](#building-for-electron)
- [Binary Management](#binary-management)
- [Path Resolution](#path-resolution)
- [Required DLLs](#required-dlls)
- [Graceful Degradation](#graceful-degradation)
- [Troubleshooting](#troubleshooting)

---

## Quick Start

```javascript
const nImage = require('./nImage/lib/index.js');

if (nImage.isLoaded) {
  const result = await nImage('photo.cr2')
    .resize(256, 256)
    .jpeg({ quality: 80 })
    .toBuffer();
}
```

---

## Building for Electron

### Step 1: Clean old binaries

```powershell
rm build/Release/nimage.node
```

The JS wrapper tries `build/Release` before `dist`. If an old binary exists there, it will be loaded instead of your new one.

### Step 2: Rebuild for Electron

```powershell
npx electron-rebuild -f -w nimage -v <electron-version>
# Example:
npx electron-rebuild -f -w nimage -v 41.1.1
```

### Step 3: Copy to dist

```powershell
cp build/Release/nimage.node dist/
cp build/Release/*.dll dist/
```

### Verify correct binary is loaded

```javascript
const fs = require('fs');
const path = require('path');
const buildPath = path.join(__dirname, 'nImage/build/Release/nimage.node');
const distPath = path.join(__dirname, 'nImage/dist/nimage.node');
console.log('build/Release:', fs.existsSync(buildPath) ? fs.statSync(buildPath).mtime : 'not found');
console.log('dist:', fs.statSync(distPath).mtime);
```

---

## Binary Management

### Recommended: Project-Level bin/ Directory

Use a `bin/` directory in your parent project as the reliable binary source.

**Project structure:**
```
YourProject/
├── bin/                    # Reliable binary location
│   ├── nimage.node
│   └── *.dll
├── nImage/                 # Submodule
├── scripts/
│   └── copy-native-bin.js
└── forge.config.js
```

**Copy script (`scripts/copy-native-bin.js`):**
```javascript
const fs = require('fs');
const path = require('path');

const sourcePaths = [
    path.join(__dirname, '..', 'nImage', 'build', 'Release', 'nimage.node'),
    path.join(__dirname, '..', 'nImage', 'dist', 'nimage.node'),
];
const targetDir = path.join(__dirname, '..', 'bin');
const targetPath = path.join(targetDir, 'nimage.node');

if (!fs.existsSync(targetDir)) fs.mkdirSync(targetDir, { recursive: true });

for (const sourcePath of sourcePaths) {
    if (fs.existsSync(sourcePath)) {
        fs.copyFileSync(sourcePath, targetPath);
        console.log(`Copied: ${sourcePath}`);
        break;
    }
}

if (process.platform === 'win32') {
    const dllSourceDir = path.join(__dirname, '..', 'nImage', 'build', 'Release');
    const dllNames = ['aom.dll', 'heif.dll', 'raw_r.dll', 'raw.dll', 'libde265.dll', 'libx265.dll', 'zlib1.dll', 'jasper.dll', 'jpeg62.dll', 'lcms2-2.dll', 'turbojpeg.dll'];
    for (const dll of dllNames) {
        const src = path.join(dllSourceDir, dll);
        const dst = path.join(targetDir, dll);
        if (fs.existsSync(src)) fs.copyFileSync(src, dst);
    }
}
```

**npm scripts:**
```json
{
  "scripts": {
    "build:native": "cd nImage && npm run build",
    "copy:native": "node scripts/copy-native-bin.js",
    "rebuild:native": "npm run build:native && npm run copy:native"
  }
}
```

**forge.config.js:**
```javascript
module.exports = {
  packagerConfig: {
    extraResource: ["config.json", "./bin/"]
  }
};
```

### Binary Loading Priority

1. `../../bin/nimage.node` — project-level bin (reliable for Electron)
2. `../build/Release/nimage.node` — development builds
3. `../prebuilds/nimage.node` — installed package
4. `../dist/nimage.node` — distribution fallback

---

## Path Resolution

### The Path Resolution Trap

Using `../nImage` in an Electron app can load from a parent directory:

```
Work_GIT/
├── nImage/                 ← WRONG binary (from parent repo)
├── BlankTest/
│   ├── js/stage.js
│   └── nImage/             ← CORRECT binary (submodule)
```

**Always use `./nImage`** to load from the current project directory.

```javascript
// WRONG - may resolve to parent directory
const nImage = require('../nImage/lib/index.js');

// CORRECT - loads from current project
const nImage = require('./nImage/lib/index.js');
```

### Adding DLL folder to PATH

```javascript
const path = require('path');
const nImageBinPath = path.resolve(appBasePath, './nImage/build/Release');
process.env.PATH = nImageBinPath + ';' + (process.env.PATH || '');

const nImage = require('./nImage/lib/index.js');
```

---

## Required DLLs

The following DLLs must be available for nImage to load on Windows:

| DLL | Source | Purpose |
|-----|--------|---------|
| `aom.dll` | vcpkg | AV1 codec for AVIF decoding |
| `heif.dll` | vcpkg | HEIC/HEIF/AVIF decoding |
| `raw_r.dll` | vcpkg | LibRaw (RAW formats) |
| `raw.dll` | vcpkg | LibRaw base |
| `libde265.dll` | vcpkg | HEVC decoder |
| `libx265.dll` | vcpkg | H.265 encoder |
| `zlib1.dll` | vcpkg | Compression |
| `jasper.dll` | vcpkg | JPEG-2000 |
| `jpeg62.dll` | vcpkg | JPEG codec |
| `lcms2-2.dll` | vcpkg | Color management |
| `turbojpeg.dll` | vcpkg | Fast JPEG |

All DLLs are automatically copied to `build/Release/` during build.

---

## Graceful Degradation

Always check `nImage.isLoaded` before using native features:

```javascript
function initNImage(appBasePath) {
    try {
        const nImageBinPath = path.resolve(appBasePath || __dirname, './nImage/build/Release');
        process.env.PATH = nImageBinPath + ';' + (process.env.PATH || '');
        const nImage = require('./nImage/lib/index.js');
        return nImage?.isLoaded ? nImage : null;
    } catch (e) {
        console.warn('nImage unavailable:', e.message);
        return null;
    }
}

const nImage = initNImage(g.basePath);
```

The JS wrapper provides these features even without the native module:
- `detectFormat()` — Pure JS format detection
- `getSupportedFormats()` — Returns format list
- `isLoaded` — `false` when native module missing
- `loadError` — Error message from failed load

---

## Troubleshooting

### Module crashes on load (0xC0000005)

**Cause**: MinGW-compiled binary used in Electron (IAT corruption).

**Fix**: Use MSVC builds only.
```powershell
npm run build  # Uses node-gyp (MSVC)
```

### Old binary keeps loading

**Cause**: Stale `build/Release/nimage.node`.

**Fix**: Delete it before rebuilding.
```powershell
rm build/Release/nimage.node
npx electron-rebuild -f -w nimage -v <version>
```

### DLL not found errors

**Cause**: DLLs not in PATH or same directory as `.node` file.

**Fix**: Add binary directory to PATH before loading:
```javascript
process.env.PATH = nImageBinPath + ';' + process.env.PATH;
```

### Sharp not available

**Cause**: Sharp is an optional dependency.

**Fix**: Install Sharp in your project:
```powershell
npm install sharp
```

Without Sharp, only native operations work (`decode`, `thumbnail`, `detectFormat`, `getMetadata`). The pipeline API requires Sharp.
