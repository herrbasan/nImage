/**
 * nImage Test Suite
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const nImage = require('..');

console.log('nImage Test Suite');
console.log('=================\n');

console.log('isLoaded:', nImage.isLoaded);
console.log('loadError:', nImage.loadError || 'none');
console.log('');

// Test format detection
console.log('--- Testing Format Detection ---');

const jpegBuffer = Buffer.from([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01]);
const heicBuffer = Buffer.from([0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63]);
const pngBuffer = Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]);

const jpegResult = nImage.detectFormat(jpegBuffer);
console.log('JPEG detection:', jpegResult.format, '(' + (jpegResult.confidence * 100).toFixed(0) + '% confidence)');
assert.strictEqual(jpegResult.format, 'jpeg');
assert.strictEqual(jpegResult.mimeType, 'image/jpeg');

const heicResult = nImage.detectFormat(heicBuffer);
console.log('HEIC detection:', heicResult.format, '(' + (heicResult.confidence * 100).toFixed(0) + '% confidence)');
assert.strictEqual(heicResult.format, 'heic');
assert.strictEqual(heicResult.mimeType, 'image/heic');

const pngResult = nImage.detectFormat(pngBuffer);
console.log('PNG detection:', pngResult.format, '(' + (pngResult.confidence * 100).toFixed(0) + '% confidence)');
assert.strictEqual(pngResult.format, 'png');
assert.strictEqual(pngResult.mimeType, 'image/png');

console.log('');

// Test supported formats
console.log('--- Testing Supported Formats ---');
const formats = nImage.getSupportedFormats();
console.log('Supported formats:', formats.length, 'formats');
console.log(formats.join(', '));
assert(Array.isArray(formats));
assert(formats.length > 0);
assert(formats.includes('cr2'));
assert(formats.includes('heic'));
assert(formats.includes('jpeg'));
console.log('');

// Test version
console.log('--- Testing Version Info ---');
console.log('Version:', nImage.version.major + '.' + nImage.version.minor + '.' + nImage.version.patch);
assert(nImage.version.major >= 0);
assert(nImage.version.minor >= 0);
assert(nImage.version.patch >= 0);
console.log('');

// Test decode (will fail without native module)
console.log('--- Testing Decode (Native Module Required) ---');
if (!nImage.isLoaded) {
    console.log('SKIPPED: Native module not built');
    console.log('Run: npm run setup && npm run build');
} else {
    try {
        const result = nImage.decode(heicBuffer);
        console.log('Decode result:', result);
    } catch (err) {
        console.log('Decode error (expected without test data):', err.message);
    }
}
console.log('');

// Summary
console.log('=================');
console.log('Basic tests PASSED');
console.log('');
if (!nImage.isLoaded) {
    console.log('NOTE: Native module not loaded. Run the following to build:');
    console.log('  npm run setup  # Download dependencies');
    console.log('  npm run build  # Compile native module');
}
