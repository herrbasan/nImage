/**
 * Build script for nImage with MSYS2/MinGW support
 *
 * This script handles building the native addon using MSYS2's MinGW compiler,
 * which is necessary when using prebuilt libraries from MSYS2.
 *
 * Run: node scripts/build.js
 */

const { spawn, execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const MSYS2_ROOT = 'C:\\msys64';
const MINGW64_BIN = path.join(MSYS2_ROOT, 'mingw64', 'bin');
const MODULE_ROOT = path.join(__dirname, '..');

console.log('nImage Build Script');
console.log('====================\n');

// Check for MSYS2
if (!fs.existsSync(MSYS2_ROOT)) {
    console.error('✗ MSYS2 not found at C:\\msys64');
    console.error('Please install MSYS2 from https://www.msys2.org/');
    console.error('Then run: node scripts/vendor.js to setup dependencies');
    process.exit(1);
}

if (!fs.existsSync(MINGW64_BIN)) {
    console.error(`✗ MSYS2 mingw64 not found at ${MINGW64_BIN}`);
    process.exit(1);
}

// Check if deps are set up
const depsInclude = path.join(MODULE_ROOT, 'deps', 'include');
const depsLib = path.join(MODULE_ROOT, 'deps', 'lib');
if (!fs.existsSync(path.join(depsLib, 'libraw.dll.a')) &&
    !fs.existsSync(path.join(depsLib, 'libraw.a'))) {
    console.warn('⚠ Dependencies may not be set up. Run: node scripts/vendor.js first.');
}

// Build function
async function build() {
    const args = process.argv.slice(2);
    const isDebug = args.includes('--debug');
    const cleanFirst = args.includes('--clean');

    // Prepare environment with MSYS2 paths
    const env = {
        ...process.env,
        PATH: `${MINGW64_BIN};${process.env.PATH}`,
        MSYSTEM: 'MINGW64',
        MSYS2_ARG_CONV_EXCL_N: '*',
    };

    // Ensure we're using Windows-style paths for node-gyp
    const moduleRootWin = MODULE_ROOT.replace(/\//g, '\\');

    console.log(`Build mode: ${isDebug ? 'debug' : 'release'}`);
    console.log(`Using MSYS2: ${MSYS2_ROOT}`);
    console.log(`Module root: ${MODULE_ROOT}\n`);

    // Clean if requested
    if (cleanFirst) {
        console.log('--- Cleaning build ---');
        await run('node', ['node-gyp', 'clean'], { env });
        console.log('');
    }

    // Configure
    console.log('--- Configuring ---');
    const configureArgs = [
        'node-gyp',
        'configure',
        '--compiler=mingw',
        `--directory=${MODULE_ROOT}`,
    ];

    try {
        await run('node', configureArgs, { env, cwd: MODULE_ROOT });
    } catch (error) {
        console.error('Configure failed. Common causes:');
        console.error('  - Missing dependencies: run node scripts/vendor.js');
        console.error('  - MSYS2 packages not installed: pacman -S mingw-w64-x86_64-libraw mingw-w64-x86_64-libheif');
        throw error;
    }
    console.log('');

    // Build
    console.log('--- Building ---');
    const buildArgs = [
        'node-gyp',
        'build',
        '--compiler=mingw',
        `--directory=${MODULE_ROOT}`,
        ...(isDebug ? ['--debug'] : ['--release']),
    ];

    await run('node', buildArgs, { env, cwd: MODULE_ROOT });
    console.log('');

    // Copy DLLs to build output
    console.log('--- Copying runtime DLLs ---');
    const buildDir = path.join(MODULE_ROOT, 'build', 'Release');
    const depsBin = path.join(MODULE_ROOT, 'deps', 'bin');

    if (fs.existsSync(depsBin)) {
        const dlls = fs.readdirSync(depsBin).filter(f => f.endsWith('.dll'));
        for (const dll of dlls) {
            const src = path.join(depsBin, dll);
            const dest = path.join(buildDir, dll);
            if (fs.existsSync(src)) {
                fs.copyFileSync(src, dest);
                console.log(`  Copied: ${dll}`);
            }
        }
    }

    // Also check deps/lib for DLLs
    const depsLibDir = path.join(MODULE_ROOT, 'deps', 'lib');
    if (fs.existsSync(depsLibDir)) {
        const dlls = fs.readdirSync(depsLibDir).filter(f => f.endsWith('.dll'));
        for (const dll of dlls) {
            const src = path.join(depsLibDir, dll);
            const dest = path.join(buildDir, dll);
            if (fs.existsSync(src)) {
                fs.copyFileSync(src, dest);
                console.log(`  Copied: ${dll}`);
            }
        }
    }

    console.log('\n=== Build complete ===');
    console.log(`Output: ${buildDir}\\nimage.node`);
}

function run(cmd, args, options = {}) {
    return new Promise((resolve, reject) => {
        console.log(`> ${cmd} ${args.join(' ')}`);

        const proc = spawn(cmd, args, {
            ...options,
            shell: true,
            windowsHide: false,
        });

        let stderr = '';
        proc.stderr.on('data', (d) => { stderr += d; });
        proc.stdout.on('data', (d) => { process.stdout.write(d); });

        proc.on('close', (code) => {
            if (code === 0) {
                resolve();
            } else {
                reject(new Error(`${cmd} exited with code ${code}\n${stderr}`));
            }
        });

        proc.on('error', reject);
    });
}

build().catch((error) => {
    console.error('\nBuild failed:', error.message);
    process.exit(1);
});
