/**
 * Build script for nImage with MSYS2/MinGW support
 *
 * This script handles building the native addon using MSYS2's MinGW compiler,
 * bypassing node-gyp which doesn't properly support MinGW on Windows.
 *
 * Run: node scripts/build.js
 */

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

const MSYS2_ROOT = 'C:\\msys64';
const MINGW64_BIN = path.join(MSYS2_ROOT, 'mingw64', 'bin');
const MSYS2_BASH = path.join(MSYS2_ROOT, 'usr', 'bin', 'bash.exe');
const MODULE_ROOT = path.join(__dirname, '..');
const BUILD_DIR = path.join(MODULE_ROOT, 'build', 'Release');

// Find Node.js headers from node-gyp cache
function getNodeHeadersPath() {
    const nodeVersion = process.versions.node;
    const cacheBase = path.join(os.homedir(), 'AppData', 'Local', 'node-gyp', 'Cache');
    const nodeIncludePath = path.join(cacheBase, `${nodeVersion}.0`, 'include', 'node');
    if (fs.existsSync(nodeIncludePath)) {
        return nodeIncludePath;
    }
    // Try to find any available version
    if (fs.existsSync(cacheBase)) {
        const dirs = fs.readdirSync(cacheBase).filter(d => d.match(/^\d+\.\d+\.\d+$/));
        if (dirs.length > 0) {
            const latest = dirs.sort().reverse()[0];
            return path.join(cacheBase, latest, 'include', 'node');
        }
    }
    return null;
}

const NODE_HEADERS_PATH = getNodeHeadersPath();

console.log('nImage Build Script');
console.log('====================\n');

// Check for MSYS2
if (!fs.existsSync(MSYS2_ROOT)) {
    console.error('MSYS2 not found at C:\\msys64');
    console.error('Please install MSYS2 from https://www.msys2.org/');
    process.exit(1);
}

if (!fs.existsSync(MINGW64_BIN)) {
    console.error(`MSYS2 mingw64 not found at ${MINGW64_BIN}`);
    process.exit(1);
}

// Check if deps are set up
const depsLib = path.join(MODULE_ROOT, 'deps', 'lib');
if (!fs.existsSync(path.join(depsLib, 'libraw.dll.a')) &&
    !fs.existsSync(path.join(depsLib, 'libraw.a'))) {
    console.warn('Dependencies may not be set up. Run: node scripts/setup.ps1 -SkipBuild first.');
}

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

    console.log(`Build mode: ${isDebug ? 'debug' : 'release'}`);
    console.log(`Using MSYS2: ${MSYS2_ROOT}`);
    console.log(`Module root: ${MODULE_ROOT}\n`);

    // Clean if requested
    if (cleanFirst) {
        console.log('--- Cleaning build ---');
        if (fs.existsSync(BUILD_DIR)) {
            fs.rmSync(BUILD_DIR, { recursive: true, force: true });
        }
        console.log('');
    }

    // Ensure build directory exists
    if (!fs.existsSync(BUILD_DIR)) {
        fs.mkdirSync(BUILD_DIR, { recursive: true });
    }

    // Get node-addon-api include path
    let napiInclude;
    try {
        napiInclude = require('node-addon-api').include;
    } catch (e) {
        console.error('Failed to get node-addon-api include path:', e.message);
        process.exit(1);
    }

    // Compiler and linker flags
    const cxxFlags = [
        '-std=c++17',
        '-DLIBHEIF_STATIC_BUILD',
        '-DLIBRAW_NODLL',
        '-fopenmp',
        '-shared',
        '-Wall',
        '-O2',
    ];

    if (isDebug) {
        cxxFlags.push('-g');
    } else {
        cxxFlags.push('-DNDEBUG');
    }

    // Convert Windows paths to MSYS2-style paths for compiler
    // D:\Work\... -> /d/Work/... (lowercase drive letter, forward slashes)
    const toMsysPath = (p) => {
        const converted = p.replace(/\\/g, '/');
        return converted.replace(/^([A-Za-z]):/, (m) => `/${m[0].toLowerCase()}`);
    };

    // Include paths
    const includePaths = [
        `-I${toMsysPath(napiInclude)}`,
        `-I${toMsysPath(path.join(MODULE_ROOT, 'src'))}`,
        `-I${toMsysPath(path.join(MODULE_ROOT, 'deps', 'include'))}`,
    ];

    // Add Node.js headers if found
    if (NODE_HEADERS_PATH) {
        includePaths.push(`-I${toMsysPath(NODE_HEADERS_PATH)}`);
        console.log(`Using Node.js headers: ${NODE_HEADERS_PATH}`);
    } else {
        console.warn('Node.js headers not found - build may fail');
    }

    // Library paths
    const libPaths = [
        `-L${toMsysPath(path.join(MODULE_ROOT, 'deps', 'lib'))}`,
        `-L${toMsysPath(path.join(MSYS2_ROOT, 'mingw64', 'lib'))}`,
        `-L${toMsysPath(path.join(MODULE_ROOT, 'build', 'Release'))}`,
        `-L${toMsysPath(path.join(process.env.LOCALAPPDATA || '', 'node-gyp', 'Cache', '39.2.3', 'x64'))}`,
    ];

    // Libraries (MinGW style)
    const libs = [
        '-l:libraw.a',
        '-l:libheif.a',
        '-lde265',
        '-laom',
        '-ldav1d',
        '-lsharpyuv',
        '-llcms2',
        '-lstdc++',
        '-fopenmp',
        '-lz',
        '-ljpeg',
        '-lws2_32',
        '-lnode',
        '-luser32',
        '-lgdi32',
    ];

    // Output
    const outputName = 'nimage.node';
    const outputPath = path.join(BUILD_DIR, outputName);

    // Source files (convert to MSYS2 paths)
    const sources = [
        toMsysPath(path.join(MODULE_ROOT, 'src', 'binding.cpp')),
        toMsysPath(path.join(MODULE_ROOT, 'src', 'decoder.cpp')),
    ];

    // Build command (use array for proper escaping)
    const buildArgs = [
        ...cxxFlags,
        ...includePaths,
        ...sources,
        ...libPaths,
        ...libs,
        `-o${toMsysPath(outputPath)}`,
    ];

    console.log('--- Building ---');
    console.log(`> g++ ${cxxFlags.join(' ')}`);
    console.log('');

    try {
        await run('g++', buildArgs, { env, cwd: MODULE_ROOT });
    } catch (error) {
        console.error('\nBuild failed:', error.message);
        process.exit(1);
    }

    console.log('');

    // Copy magick.exe to dist for CLI fallback
    const depsBin = path.join(MODULE_ROOT, 'deps', 'bin');
    const distDir = path.join(MODULE_ROOT, 'dist');
    if (!fs.existsSync(distDir)) {
        fs.mkdirSync(distDir, { recursive: true });
    }

    const magickPath = path.join(depsBin, 'magick.exe');
    if (fs.existsSync(magickPath)) {
        fs.copyFileSync(magickPath, path.join(distDir, 'magick.exe'));
        console.log(`  Copied: magick.exe to dist`);
    }

    const prefixBin = path.join(MSYS2_ROOT, 'mingw64', 'bin');
    ['libde265', 'libaom', 'libdav1d', 'libsharpyuv', 'libjpeg', 'libgomp', 'zlib', 'libgcc_s_seh', 'libstdc++', 'libwinpthread', 'liblcms2'].forEach(dllPrefix => {
        const matchingDlls = fs.readdirSync(prefixBin).filter(f => f.startsWith(dllPrefix) && f.endsWith('.dll'));
        for (const dll of matchingDlls) {
            fs.copyFileSync(path.join(prefixBin, dll), path.join(distDir, dll));
            console.log(`  Copied: ${dll} to dist`);
        }
    });

    // Copy the module
    fs.copyFileSync(outputPath, path.join(distDir, 'nimage.node'));
    console.log(`  Copied: nimage.node`);

    console.log('\n=== Build complete ===');
    console.log(`Output: ${outputPath}`);
    console.log(`Dist: ${distDir}`);
}

function run(cmd, args, options = {}) {
    return new Promise((resolve, reject) => {
        // On Windows, use MSYS2 bash for proper MinGW environment
        const escapeArg = (arg) => {
            if (arg.includes(' ') || arg.includes('(') || arg.includes(')') || arg.includes('&')) {
                return `"${arg.replace(/"/g, '\\"')}"`;
            }
            return arg;
        };

        const cmdStr = `${cmd} ${args.map(escapeArg).join(' ')}`;
        console.log(`> ${cmdStr}`);

        // Use MSYS2 bash to run the command (ensures proper PATH for MinGW tools)
        const bashArgs = ['-c', cmdStr];

        const proc = spawn(MSYS2_BASH, bashArgs, {
            ...options,
            env: {
                ...options.env,
                MSYSTEM: 'MINGW64',
                PATH: `${path.join(MSYS2_ROOT, 'mingw64', 'bin')};${path.join(MSYS2_ROOT, 'usr', 'bin')};${process.env.PATH}`,
                MSYS2_ARG_CONV_EXCL_N: '*',
            },
            windowsHide: false,
        });

        let stderr = '';
        proc.stderr.on('data', (d) => { stderr += d; process.stderr.write(d); });
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