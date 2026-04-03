/**
 * nImage Benchmark Suite
 * Tests format detection and decode performance
 */

const fs = require('fs');
const path = require('path');
const nImage = require('..');

console.log('nImage Benchmark Suite');
console.log('========================\n');

console.log('Module loaded:', nImage.isLoaded);
console.log('');

// Find test images
const testDirs = [
    path.join(__dirname, '..', '..', 'nui_wc2', 'assets'),
    path.join(__dirname, '..', '..', 'ffmpeg-napi-interface'),
    __dirname,
];

const imageExtensions = ['.jpg', '.jpeg', '.png', '.webp', '.tiff', '.tif', '.heic', '.heif'];
let testImages = [];

for (const dir of testDirs) {
    if (fs.existsSync(dir)) {
        try {
            const files = fs.readdirSync(dir);
            for (const file of files) {
                const ext = path.extname(file).toLowerCase();
                if (imageExtensions.includes(ext)) {
                    testImages.push(path.join(dir, file));
                }
            }
        } catch (e) {
            // Skip directories we can't read
        }
    }
}

// Deduplicate
testImages = [...new Set(testImages)];

console.log(`Found ${testImages.length} test images\n`);

// ============================================================================
// Test Buffers (various formats)
// ============================================================================
const testBuffers = {
    jpeg: Buffer.from([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01]),
    png: Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D]),
    heic: Buffer.from([0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63]),
    webp: Buffer.from([0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50]),
    tiff: Buffer.from([0x49, 0x49, 0x2A, 0x00]), // Little-endian TIFF
    dng: Buffer.from([0x4D, 0x4D, 0x00, 0x2A]), // Big-endian TIFF (DNG often)
};

// ============================================================================
// Benchmark: Format Detection (Small Buffers)
// ============================================================================
console.log('--- Format Detection Benchmark (Small Buffers) ---');

const iterations = 50000;

console.log(`\nIterations: ${iterations.toLocaleString()}\n`);

// Warmup
for (const [name, buf] of Object.entries(testBuffers)) {
    for (let i = 0; i < 100; i++) {
        nImage.detectFormat(buf);
    }
}

// Benchmark each format
const detectionResults = {};

for (const [name, buf] of Object.entries(testBuffers)) {
    const start = process.hrtime.bigint();
    for (let i = 0; i < iterations; i++) {
        nImage.detectFormat(buf);
    }
    const end = process.hrtime.bigint();
    const time = Number(end - start) / 1e6;
    const perCall = time / iterations;
    const opsPerSec = Math.round(iterations / (time / 1000));

    detectionResults[name] = { time, perCall, opsPerSec };

    const result = nImage.detectFormat(buf);
    console.log(`${name.toUpperCase()} (${buf.length} bytes):`);
    console.log(`  Detected: ${result.format}`);
    console.log(`  Total: ${time.toFixed(2)} ms, Per call: ${(perCall * 1000).toFixed(4)} µs, Ops/sec: ${opsPerSec.toLocaleString()}`);
}

// ============================================================================
// Benchmark: Large Buffer Detection
// ============================================================================
console.log('\n--- Large Buffer Detection ---');

let largeBuffer = Buffer.from([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01]);

if (testImages.length > 0) {
    try {
        largeBuffer = fs.readFileSync(testImages[0]);
        console.log(`Using: ${path.basename(testImages[0])} (${(largeBuffer.length / 1024).toFixed(1)} KB)`);
    } catch (e) {
        console.log('Using synthetic JPEG buffer');
    }
} else {
    // Pad to simulate a real JPEG
    largeBuffer = Buffer.alloc(100 * 1024, 0xFF);
    largeBuffer[0] = 0xFF;
    largeBuffer[1] = 0xD8;
    largeBuffer[2] = 0xFF;
    console.log(`Using synthetic buffer (${(largeBuffer.length / 1024).toFixed(1)} KB)`);
}

const largeIterations = 10000;

const largeStart = process.hrtime.bigint();
for (let i = 0; i < largeIterations; i++) {
    nImage.detectFormat(largeBuffer);
}
const largeEnd = process.hrtime.bigint();
const largeTime = Number(largeEnd - largeStart) / 1e6;

console.log(`\nLarge buffer (${largeBuffer.length} bytes):`);
console.log(`  Total: ${largeTime.toFixed(2)} ms`);
console.log(`  Per call: ${(largeTime / largeIterations * 1000).toFixed(4)} µs`);
console.log(`  Ops/sec: ${Math.round(largeIterations / (largeTime / 1000)).toLocaleString()}`);

// ============================================================================
// Benchmark: getSupportedFormats
// ============================================================================
console.log('\n--- getSupportedFormats Benchmark ---');

const formatIterations = 100000;

const formatStart = process.hrtime.bigint();
for (let i = 0; i < formatIterations; i++) {
    nImage.getSupportedFormats();
}
const formatEnd = process.hrtime.bigint();
const formatTime = Number(formatEnd - formatStart) / 1e6;

console.log(`Iterations: ${formatIterations.toLocaleString()}`);
console.log(`Total: ${formatTime.toFixed(2)} ms`);
console.log(`Per call: ${(formatTime / formatIterations * 1000).toFixed(4)} µs`);
console.log(`Ops/sec: ${Math.round(formatIterations / (formatTime / 1000)).toLocaleString()}`);

// ============================================================================
// Benchmark: Image Analysis (Real Files)
// ============================================================================
if (testImages.length > 0) {
    console.log('\n--- Image Analysis (Real Files) ---');

    for (const imgPath of testImages.slice(0, 5)) {
        const buffer = fs.readFileSync(imgPath);
        const result = nImage.detectFormat(buffer);
        const fileSize = buffer.length;

        console.log(`\n${path.basename(imgPath)}:`);
        console.log(`  Size: ${(fileSize / 1024).toFixed(1)} KB`);
        console.log(`  Format: ${result.format} (${(result.confidence * 100).toFixed(0)}%)`);

        // Time individual detection
        const detectStart = process.hrtime.bigint();
        for (let i = 0; i < 1000; i++) {
            nImage.detectFormat(buffer);
        }
        const detectEnd = process.hrtime.bigint();
        const detectTime = Number(detectEnd - detectStart) / 1e6;

        console.log(`  Detection time: ${(detectTime / 1000 * 1000).toFixed(4)} µs per call`);
    }
}

// ============================================================================
// Summary
// ============================================================================
console.log('\n========================');
console.log('Benchmark complete');
console.log(`Tested ${testImages.length} images`);
console.log(`Module: ${nImage.isLoaded ? 'Native (NAPI)' : 'JavaScript fallback'}`);