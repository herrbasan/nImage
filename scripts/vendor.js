/**
 * Vendor script to set up nImage dependencies
 *
 * Automatically detects and uses MSYS2 (recommended) for prebuilt binaries,
 * or creates a deps/ structure for manual setup.
 *
 * Run: node scripts/vendor.js
 */

const fs = require('fs');
const path = require('path');
const { spawn, execSync } = require('child_process');
const os = require('os');

const DEPS_DIR = path.join(__dirname, '..', 'deps');
const EXTRACT_DIR = path.join(DEPS_DIR, 'extracted');
const MSYS2_ROOT_KEY = 'C:\\msys64';
const MINGW64 = path.join(MSYS2_ROOT_KEY, 'mingw64');

// Required MSYS2 packages for nImage
const MSYS2_PACKAGES = [
    'mingw-w64-x86_64-libraw',
    'mingw-w64-x86_64-libheif',
    'mingw-w64-x86_64-imagemagick',
    // libheif dependencies (auto-installed as deps, but listed for clarity)
    'mingw-w64-x86_64-libde265',
    'mingw-w64-x86_64-aom',
    'mingw-w64-x86_64-x265',
    'mingw-w64-x86_64-libjpeg-turbo',
    'mingw-w64-x86_64-libtiff',
    'mingw-w64-x86_64-zlib',
    'mingw-w64-x86_64-libpng',
];

console.log('nImage Vendor Setup Script');
console.log('==========================\n');

// ============================================================================
// MSYS2 Detection and Setup
// ============================================================================

function findMSYS2() {
    const possiblePaths = [
        'C:\\msys64',
        'C:\\msys32',
        path.join(process.env.LOCALAPPDATA || '', 'msys64'),
        path.join(process.env.PROGRAMDATA || 'C:\\ProgramData', 'msys64'),
    ];

    // Also check common Unix-style paths (for Git Bash / WSL)
    if (os.platform() !== 'win32') {
        possiblePaths.push('/usr', '/opt/local');
    }

    for (const p of possiblePaths) {
        if (p && fs.existsSync(p) && fs.existsSync(path.join(p, 'mingw64'))) {
            return p;
        }
    }
    return null;
}

function runPacman(msysRoot, args, options = {}) {
    return new Promise((resolve, reject) => {
        const pacmanBin = path.join(msysRoot, 'usr', 'bin', 'pacman.exe');
        const mingwBin = path.join(msysRoot, 'mingw64', 'bin');

        // Run in MSYS2 environment with MSYS2_ARG_CONV_EXCL_N to prevent path conversion
        const env = {
            ...process.env,
            MSYS2_ARG_CONV_EXCL_N: '*',  // Prevent MSYS2 from converting paths
            MSYSTEM: 'MINGW64',
            PATH: `${mingwBin};${path.join(msysRoot, 'usr', 'bin')};${process.env.PATH}`,
        };

        const proc = spawn(pacmanBin, args, {
            ...options,
            env,
            shell: false,
            windowsHide: true,
        });

        let stdout = '';
        let stderr = '';

        proc.stdout.on('data', (d) => { stdout += d; process.stdout.write(d); });
        proc.stderr.on('data', (d) => { stderr += d; process.stderr.write(d); });

        proc.on('close', (code) => {
            if (code === 0) {
                resolve({ stdout, stderr, code });
            } else {
                reject(new Error(`pacman exited with code ${code}\n${stderr}`));
            }
        });

        proc.on('error', reject);
    });
}

async function checkMSYS2Packages(msysRoot) {
    console.log('\n--- Checking MSYS2 packages ---');

    const pacmanBin = path.join(msysRoot, 'usr', 'bin', 'pacman.exe');
    const mingwBin = path.join(msysRoot, 'mingw64', 'bin');

    // Query installed packages
    const env = {
        ...process.env,
        MSYS2_ARG_CONV_EXCL_N: '*',
        MSYSTEM: 'MINGW64',
        PATH: `${mingwBin};${path.join(msysRoot, 'usr', 'bin')};${process.env.PATH}`,
    };

    let installedPkgs;
    try {
        // pacman -Q without args lists all installed packages (format: "name version")
        installedPkgs = execSync(
            `"${pacmanBin}" -Q`,
            { env, encoding: 'utf8', windowsHide: true }
        );
    } catch (e) {
        return {};
    }

    const pkgMap = {};
    for (const line of installedPkgs.trim().split('\n')) {
        const parts = line.split(' ');
        if (parts.length >= 2) {
            const name = parts[0];
            const version = parts[1];
            pkgMap[name] = version;
        }
    }

    const missing = [];
    for (const pkg of MSYS2_PACKAGES) {
        if (!pkgMap[pkg]) {
            missing.push(pkg);
        } else {
            console.log(`  ✓ ${pkg} (${pkgMap[pkg]})`);
        }
    }

    return missing;
}

async function installMSYS2Packages(msysRoot, packages) {
    if (packages.length === 0) return;

    console.log(`\n--- Installing MSYS2 packages ---`);
    console.log(`Packages to install: ${packages.join(', ')}`);

    // Update package database first
    console.log('\nUpdating package database...');
    await runPacman(msysRoot, ['-Sy']);

    console.log('\nInstalling packages...');
    await runPacman(msysRoot, ['-S', '--noconfirm', ...packages]);
}

async function copyMSYS2Libs(msysRoot) {
    console.log('\n--- Copying MSYS2 libraries to deps/ ---');

    const srcMingw = path.join(msysRoot, 'mingw64');
    const destInclude = path.join(DEPS_DIR, 'include');
    const destLib = path.join(DEPS_DIR, 'lib');
    const destBin = path.join(DEPS_DIR, 'bin');

    // Clean and create directories
    for (const d of [destInclude, destLib, destBin]) {
        if (fs.existsSync(d)) {
            fs.rmSync(d, { recursive: true, force: true });
        }
        fs.mkdirSync(d, { recursive: true });
    }

    // Headers from mingw64/include
    const srcInclude = path.join(srcMingw, 'include');
    if (fs.existsSync(srcInclude)) {
        copyDir(srcInclude, destInclude);
        console.log('  Copied headers');
    }

    // Libs from mingw64/lib
    const srcLib = path.join(srcMingw, 'lib');
    if (fs.existsSync(srcLib)) {
        // Copy only .a and .dll.a files (static import libs)
        const libFiles = fs.readdirSync(srcLib).filter(f =>
            f.endsWith('.a') || f.endsWith('.dll.a') || f.endsWith('.lib')
        );
        for (const f of libFiles) {
            fs.copyFileSync(path.join(srcLib, f), path.join(destLib, f));
        }
        console.log(`  Copied ${libFiles.length} library files`);
    }

    // DLLs from mingw64/bin - copy ALL to ensure all transitive deps are present
    const srcBin = path.join(srcMingw, 'bin');
    if (fs.existsSync(srcBin)) {
        // Copy all DLLs to ensure all transitive dependencies are present
        // libheif depends on: libde265, aom, x265, rav1e, svt-av1, dav1d, and their deps
        const binFiles = fs.readdirSync(srcBin).filter(f => f.endsWith('.dll'));
        for (const f of binFiles) {
            fs.copyFileSync(path.join(srcBin, f), path.join(destBin, f));
        }
        console.log(`  Copied ${binFiles.length} DLL files (all transitive dependencies)`);
    }

    console.log('  Done copying libraries');
}

function copyDir(src, dest) {
    if (!fs.existsSync(src)) return;

    const entries = fs.readdirSync(src, { withFileTypes: true });
    for (const entry of entries) {
        const srcPath = path.join(src, entry.name);
        const destPath = path.join(dest, entry.name);

        if (entry.isDirectory()) {
            if (!fs.existsSync(destPath)) {
                fs.mkdirSync(destPath, { recursive: true });
            }
            copyDir(srcPath, destPath);
        } else {
            // Only copy header files
            if (entry.name.endsWith('.h') || entry.name.endsWith('.hpp')) {
                fs.copyFileSync(srcPath, destPath);
            }
        }
    }
}

// ============================================================================
// Main Setup Flow
// ============================================================================

async function main() {
    console.log(`Platform: ${os.platform()} ${os.arch()}`);
    console.log(`Node.js: ${process.version}`);
    console.log(`Target dir: ${DEPS_DIR}\n`);

    // Ensure deps directory exists
    if (!fs.existsSync(DEPS_DIR)) {
        fs.mkdirSync(DEPS_DIR, { recursive: true });
    }

    // Try to find MSYS2
    const msysRoot = findMSYS2();

    if (msysRoot) {
        console.log(`✓ MSYS2 found at: ${msysRoot}`);

        try {
            // Check which packages are missing
            const missing = await checkMSYS2Packages(msysRoot);

            if (missing.length > 0) {
                console.log(`\n⚠ Missing packages: ${missing.join(', ')}`);
                const install = await askYesNo('Install missing packages with pacman?');
                if (install) {
                    await installMSYS2Packages(msysRoot, missing);
                } else {
                    console.log('Skipping package installation.');
                    console.log('You can install manually with: pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif');
                }
            } else {
                console.log('\n✓ All required packages are installed');
            }

            // Copy libraries to deps/
            await copyMSYS2Libs(msysRoot);

            console.log('\n=== MSYS2 Setup Complete ===');
            printNextSteps(true);
            return;

        } catch (error) {
            console.error(`\n✗ MSYS2 setup failed: ${error.message}`);
            console.log('Falling back to manual setup...\n');
        }
    } else {
        console.log('✗ MSYS2 not found');
        console.log('\nMSYS2 is recommended for prebuilt dependencies.');
        console.log('Download from: https://www.msys2.org/');
        console.log('After installation, run this script again.\n');
    }

    // Fallback: create placeholder structure
    createPlaceholderStructure();
    printNextSteps(false);
}

function createPlaceholderStructure() {
    console.log('\n--- Creating placeholder deps structure ---');

    const dirs = [
        path.join(DEPS_DIR, 'libraw', 'include'),
        path.join(DEPS_DIR, 'libheif', 'include'),
        path.join(DEPS_DIR, 'lib'),
        path.join(DEPS_DIR, 'bin'),
    ];

    for (const d of dirs) {
        fs.mkdirSync(d, { recursive: true });
    }

    // Create README files with instructions
    fs.writeFileSync(
        path.join(DEPS_DIR, 'README.md'),
        `# nImage Dependencies

This directory contains or will contain native library dependencies.

## Option 1: MSYS2 (Recommended)

Install MSYS2 from https://www.msys2.org/, then run:

\`\`\`bash
# In MSYS2 MINGW64 terminal:
pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif
\`\`\`

Then re-run: \`node scripts/vendor.js\`

## Option 2: Manual

Download and extract libraries to:
- Headers: deps/libraw/include/, deps/libheif/include/
- Libs: deps/lib/
- DLLs: deps/bin/

## Option 3: vcpkg

\`\`\`bash
vcpkg install libraw:x64-windows libheif:x64-windows
# Copy from vcpkg_installed/x64-windows/
\`\`\`
`
    );

    fs.writeFileSync(
        path.join(DEPS_DIR, 'libraw', 'include', 'README.md'),
        `# libraw Placeholder

libraw headers should be placed in this directory.

Download: https://www.libraw.org/
Or install via MSYS2: \`pacman -S mingw-w64-x86_64-libraw\`
`
    );

    fs.writeFileSync(
        path.join(DEPS_DIR, 'libheif', 'include', 'README.md'),
        `# libheif Placeholder

libheif headers should be placed in this directory.

Download: https://github.com/strukturag/libheif
Or install via MSYS2: \`pacman -S mingw-w64-x86_64-libheif\`
`
    );

    console.log('Created placeholder structure in deps/');
}

function printNextSteps(hasMSYS2) {
    console.log('\n=== Next Steps ===');
    if (hasMSYS2) {
        console.log('1. Build the native module:');
        console.log('   cd modules/nImage');
        console.log('   npm run build');
    } else {
        console.log('1. Install MSYS2 (recommended):');
        console.log('   https://www.msys2.org/');
        console.log('   Then run: node scripts/vendor.js');
        console.log('');
        console.log('2. Or install dependencies manually and update binding.gyp');
    }
    console.log('');
    console.log('3. Run tests:');
    console.log('   npm run test');
}

function askYesNo(question) {
    return new Promise((resolve) => {
        const readline = require('readline');
        const rl = readline.createInterface({
            input: process.stdin,
            output: process.stdout
        });

        rl.question(`${question} (y/N) `, (answer) => {
            rl.close();
            resolve(answer.toLowerCase() === 'y');
        });
    });
}

main().catch((error) => {
    console.error('\nFatal error:', error);
    process.exit(1);
});
