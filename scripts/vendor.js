/**
 * Vendor script to download required libraries for nImage
 *
 * Downloads and extracts:
 * - libraw: For RAW image decoding (CR2, NEF, ARW, ORF, etc.)
 * - libheif: For HEIC/HEIF image decoding
 *
 * Run: node scripts/vendor.js
 */

const fs = require('fs');
const path = require('path');
const https = require('https');
const { spawn } = require('child_process');
const os = require('os');

const VENDOR_DIR = path.join(__dirname, '..', 'deps');
const EXTRACT_DIR = path.join(__dirname, '..', 'deps', 'extracted');

console.log('nImage Vendor Download Script');
console.log('=============================\n');

// Ensure directories exist
if (!fs.existsSync(VENDOR_DIR)) {
    fs.mkdirSync(VENDOR_DIR, { recursive: true });
}

if (!fs.existsSync(EXTRACT_DIR)) {
    fs.mkdirSync(EXTRACT_DIR, { recursive: true });
}

// ============================================================================
// Library URLs (using vcpkg prebuilt binaries where available)
// ============================================================================

const LIBRAW_VERSION = '0.21.2';
const LIBHEIF_VERSION = '1.18.2';

// These URLs point to vcpkg baseline releases - you may need to find
// actual prebuilt binaries or build from source
const LIBRAW_URLS = {
    win64: `https://github.com/microsoft/vcpkg/releases/download/2024.08.19/libraw_x64-windows-${LIBRAW_VERSION}.zip`,
    linux: `https://github.com/microsoft/vcpkg/releases/download/2024.08.19/libraw_x64-linux-${LIBRAW_VERSION}.tar.gz`
};

const LIBHEIF_URLS = {
    win64: `https://github.com/microsoft/vcpkg/releases/download/2024.08.19/libheif_x64-windows-${LIBHEIF_VERSION}.zip`,
    linux: `https://github.com/microsoft/vcpkg/releases/download/2024.08.19/libheif_x64-linux-${LIBHEIF_VERSION}.tar.gz`
};

// ============================================================================
// Utility Functions
// ============================================================================

function getPlatform() {
    const platform = os.platform();
    const arch = os.arch();
    if (platform === 'win32') return 'win64';
    if (platform === 'linux') return 'linux';
    return null;
}

function downloadFile(url, destPath) {
    return new Promise((resolve, reject) => {
        console.log(`Downloading: ${url}`);
        console.log(`Destination: ${destPath}`);

        const file = fs.createWriteStream(destPath);

        https.get(url, (response) => {
            // Handle redirects
            if (response.statusCode === 302 || response.statusCode === 301) {
                console.log('Following redirect...');
                https.get(response.headers.location, (redirectResponse) => {
                    pipeAndSave(redirectResponse, file, destPath, resolve, reject);
                }).on('error', reject);
            } else {
                pipeAndSave(response, file, destPath, resolve, reject);
            }
        }).on('error', reject);
    });
}

function pipeAndSave(response, file, destPath, resolve, reject) {
    let downloaded = 0;
    const totalSize = parseInt(response.headers['content-length'] || '0', 10);

    response.on('data', (chunk) => {
        downloaded += chunk.length;
        if (totalSize > 0) {
            const percent = ((downloaded / totalSize) * 100).toFixed(1);
            process.stdout.write(`\rProgress: ${percent}%`);
        }
    });

    response.pipe(file);

    file.on('finish', () => {
        file.close();
        console.log('\nDownload complete!');
        resolve();
    });

    response.on('error', (err) => {
        fs.unlinkSync(destPath);
        reject(err);
    });
}

function extractZip(zipPath, destDir) {
    return new Promise((resolve, reject) => {
        console.log(`Extracting: ${zipPath}`);

        const psCommand = `
            $zip = "${zipPath.replace(/\\/g, '\\\\')}"
            $dest = "${destDir.replace(/\\/g, '\\\\')}"
            if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
            Expand-Archive -Path $zip -DestinationPath $dest -Force
        `;

        const ps = spawn('powershell', ['-ExecutionPolicy', 'Bypass', '-Command', psCommand]);

        ps.stdout.on('data', (data) => console.log(data.toString().trim()));
        ps.stderr.on('data', (data) => console.error(data.toString().trim()));

        ps.on('close', (code) => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`Extract failed with code ${code}`));
            }
        });
    });
}

function extractTarGz(tarPath, destDir) {
    return new Promise((resolve, reject) => {
        console.log(`Extracting: ${tarPath}`);

        const tar = spawn('tar', ['-xzf', tarPath, '-C', destDir]);

        tar.stdout.on('data', (data) => process.stdout.write(data));
        tar.stderr.on('data', (data) => process.stderr.write(data));

        tar.on('close', (code) => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`Extract failed with code ${code}`));
            }
        });
    });
}

function copyLibs(srcDir, destDir, ext) {
    console.log(`Copying ${ext} files from ${srcDir} to ${destDir}`);

    if (!fs.existsSync(destDir)) {
        fs.mkdirSync(destDir, { recursive: true });
    }

    const files = fs.readdirSync(srcDir);
    for (const file of files) {
        if (file.endsWith(ext)) {
            const src = path.join(srcDir, file);
            const dest = path.join(destDir, file);
            fs.copyFileSync(src, dest);
            console.log(`  Copied: ${file}`);
        }
    }
}

function cleanDir(dir) {
    if (fs.existsSync(dir)) {
        fs.rmSync(dir, { recursive: true, force: true });
    }
    fs.mkdirSync(dir, { recursive: true });
}

// ============================================================================
// Main Download Functions
// ============================================================================

async function downloadLibraw(platform) {
    const libName = 'libraw';
    const zipPath = path.join(VENDOR_DIR, `${libName}.zip`);
    const extractPlatformDir = path.join(EXTRACT_DIR, `${libName}-${platform}`);

    // Check if already downloaded
    const winLibDir = path.join(VENDOR_DIR, 'win', 'lib');
    const linuxLibDir = path.join(VENDOR_DIR, 'linux', 'lib');

    if (platform === 'win64' && fs.existsSync(winLibDir)) {
        console.log('LibRaw already present (win64)');
        return;
    }
    if (platform === 'linux' && fs.existsSync(linuxLibDir)) {
        console.log('LibRaw already present (linux)');
        return;
    }

    console.log(`\n--- Downloading LibRaw ---`);

    // Note: These URLs may not exist. In practice, you'd need to:
    // 1. Use vcpkg to build and install libraw
    // 2. Download from a trusted source that provides prebuilt binaries
    // 3. Build from source
    //
    // For now, we create placeholder structure
    console.log('Note: LibRaw binaries need to be obtained via vcpkg or built from source');
    console.log('Expected include dir: deps/libraw/');
    console.log('Expected lib dir: deps/win/lib/ or deps/linux/lib/');

    // Create placeholder directories
    cleanDir(path.join(VENDOR_DIR, 'libraw'));
    fs.mkdirSync(path.join(VENDOR_DIR, 'libraw', 'include'), { recursive: true });
    fs.mkdirSync(path.join(VENDOR_DIR, 'win', 'lib'), { recursive: true });
    fs.mkdirSync(path.join(VENDOR_DIR, 'linux', 'lib'), { recursive: true });

    // Create a README in libraw include dir
    fs.writeFileSync(
        path.join(VENDOR_DIR, 'libraw', 'include', 'README.md'),
        `# LibRaw ${LIBRAW_VERSION}

To obtain LibRaw:
1. Use vcpkg: vcpkg install libraw
2. Copy headers to this directory
3. Copy libraries to deps/win/lib/ or deps/linux/lib/

Headers needed:
- libraw.h
- libraw_types.h
- libraw_const.h
- utils/libraw_alloc.h
`
    );

    console.log('LibRaw placeholder structure created');
}

async function downloadLibheif(platform) {
    const libName = 'libheif';
    const zipPath = path.join(VENDOR_DIR, `${libName}.zip`);
    const extractPlatformDir = path.join(EXTRACT_DIR, `${libName}-${platform}`);

    // Check if already downloaded
    const winLibDir = path.join(VENDOR_DIR, 'win', 'lib');
    const linuxLibDir = path.join(VENDOR_DIR, 'linux', 'lib');

    if (platform === 'win64' && fs.existsSync(winLibDir)) {
        console.log('LibHeif already present (win64)');
        return;
    }
    if (platform === 'linux' && fs.existsSync(linuxLibDir)) {
        console.log('LibHeif already present (linux)');
        return;
    }

    console.log(`\n--- Downloading LibHeif ---`);

    // Note: Similar to LibRaw, prebuilt binaries may not be readily available
    console.log('Note: LibHeif binaries need to be obtained via vcpkg or built from source');
    console.log('Expected include dir: deps/libheif/include/');
    console.log('Expected lib dir: deps/win/lib/ or deps/linux/lib/');

    // Create placeholder directories
    cleanDir(path.join(VENDOR_DIR, 'libheif'));
    fs.mkdirSync(path.join(VENDOR_DIR, 'libheif', 'include'), { recursive: true });
    fs.mkdirSync(path.join(VENDOR_DIR, 'win', 'lib'), { recursive: true });
    fs.mkdirSync(path.join(VENDOR_DIR, 'linux', 'lib'), { recursive: true });

    // Create a README in libheif include dir
    fs.writeFileSync(
        path.join(VENDOR_DIR, 'libheif', 'include', 'README.md'),
        `# LibHeif ${LIBHEIF_VERSION}

To obtain LibHeif:
1. Use vcpkg: vcpkg install libheif
2. Copy headers to this directory
3. Copy libraries to deps/win/lib/ or deps/linux/lib/

Headers needed:
- heif.h

Note: libheif depends on:
- libde265 (for H.265/HEVC support)
- libx265 (optional, for HEVC encoding)
- aom (for AVIF support)
`
    );

    console.log('LibHeif placeholder structure created');
}

// ============================================================================
// Main
// ============================================================================

async function main() {
    const platform = getPlatform();

    if (!platform) {
        console.error(`Unsupported platform: ${os.platform()}`);
        console.error('Supported platforms: win32, linux');
        process.exit(1);
    }

    console.log(`Platform: ${platform}`);
    console.log(`Vendor dir: ${VENDOR_DIR}\n`);

    try {
        await downloadLibraw(platform);
        await downloadLibheif(platform);

        console.log('\n=== Vendor Setup Complete ===');
        console.log('');
        console.log('Next steps:');
        console.log('1. Install dependencies via vcpkg:');
        console.log('   vcpkg install libraw libheif');
        console.log('');
        console.log('2. Or build from source and copy headers/libs to deps/');
        console.log('');
        console.log('3. Then run: npm run build');
        console.log('');

    } catch (error) {
        console.error('\nError:', error.message);
        process.exit(1);
    }
}

main();
