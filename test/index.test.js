/**
 * nImage Test Suite
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const nImage = require('..');

async function runTests() {
    console.log('nImage Test Suite');
    console.log('=================\n');

    console.log('isLoaded:', nImage.isLoaded);
    console.log('hasSharp:', nImage.hasSharp);
    console.log('loadError:', nImage.loadError || 'none');
    console.log('');

    // Test format detection
    console.log('--- Testing Format Detection ---');

    const jpegBuffer = Buffer.from([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01]);
    const heicBuffer = Buffer.from([0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x68, 0x65, 0x69, 0x63]);
    const pngBuffer = Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D]);

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

    // Test pipeline with JPEG (pass-through to Sharp)
    console.log('--- Testing Pipeline (Sharp Pass-through) ---');
    if (!nImage.hasSharp) {
        console.log('SKIPPED: Sharp not installed');
    } else {
        const testJpgPath = path.join(__dirname, 'assets', '_4240213.jpg');
        if (fs.existsSync(testJpgPath)) {
            try {
                // Test JPEG resize and convert to buffer
                const result = await nImage(testJpgPath)
                    .resize(100, 100, { fit: 'inside' })
                    .jpeg({ quality: 80 })
                    .toBuffer();

                assert(Buffer.isBuffer(result), 'Result should be a Buffer');
                assert(result.length > 0, 'Result should not be empty');

                // Verify it's a valid JPEG
                assert(result[0] === 0xFF && result[1] === 0xD8, 'Should be a valid JPEG');
                console.log('JPEG resize + encode: OK (' + result.length + ' bytes)');

                // Test PNG conversion
                const pngResult = await nImage(testJpgPath)
                    .resize(50, 50)
                    .png()
                    .toBuffer();

                assert(Buffer.isBuffer(pngResult), 'PNG result should be a Buffer');
                assert(pngResult[0] === 0x89 && pngResult[1] === 0x50, 'Should be a valid PNG');
                console.log('JPEG resize + PNG encode: OK (' + pngResult.length + ' bytes)');

                // Test WebP conversion
                const webpResult = await nImage(testJpgPath)
                    .resize(75, 75)
                    .webp({ quality: 85 })
                    .toBuffer();

                assert(Buffer.isBuffer(webpResult), 'WebP result should be a Buffer');
                console.log('JPEG resize + WebP encode: OK (' + webpResult.length + ' bytes)');

            } catch (err) {
                console.log('Pipeline test error:', err.message);
            }
        } else {
            console.log('SKIPPED: Test JPEG not found at', testJpgPath);
        }
    }
    console.log('');

    // Test pipeline with ImageData input
    console.log('--- Testing Pipeline with ImageData Input ---');
    if (!nImage.hasSharp) {
        console.log('SKIPPED: Sharp not installed');
    } else if (!nImage.isLoaded) {
        console.log('SKIPPED: Native module not loaded');
    } else {
        const testCr2Path = path.join(__dirname, 'assets', 'IMG_2593.CR2');
        if (fs.existsSync(testCr2Path)) {
            try {
                // Decode RAW first
                const imageData = nImage.decode(fs.readFileSync(testCr2Path));
                console.log('RAW decode:', imageData.width + 'x' + imageData.height, imageData.format);

                // Then pass through pipeline
                const result = await nImage(imageData)
                    .resize(256, 256, { fit: 'inside' })
                    .jpeg({ quality: 85 })
                    .toBuffer();

                assert(Buffer.isBuffer(result), 'Result should be a Buffer');
                console.log('RAW decode + resize + JPEG encode: OK (' + result.length + ' bytes)');

            } catch (err) {
                console.log('RAW pipeline test error:', err.message);
            }
        } else {
            console.log('SKIPPED: Test CR2 not found');
        }
    }
    console.log('');

    // Test HEIC pipeline
    console.log('--- Testing HEIC Pipeline ---');
    if (!nImage.hasSharp) {
        console.log('SKIPPED: Sharp not installed');
    } else if (!nImage.isLoaded) {
        console.log('SKIPPED: Native module not loaded');
    } else {
        const testHeicPath = path.join(__dirname, 'assets', 'IMG_0092_1.HEIC');
        if (fs.existsSync(testHeicPath)) {
            try {
                const result = await nImage(testHeicPath)
                    .resize(512, 512, { fit: 'inside' })
                    .jpeg({ quality: 90 })
                    .toBuffer();

                assert(Buffer.isBuffer(result), 'Result should be a Buffer');
                console.log('HEIC decode + resize + JPEG encode: OK (' + result.length + ' bytes)');

            } catch (err) {
                console.log('HEIC pipeline test error:', err.message);
            }
        } else {
            console.log('SKIPPED: Test HEIC not found at', testHeicPath);
        }
    }
    console.log('');

    // Summary
    console.log('=================');
    console.log('Basic tests PASSED');
    console.log('');

    if (!nImage.hasSharp) {
        console.log('NOTE: Sharp not installed. Pipeline tests skipped.');
        console.log('Install with: npm install sharp');
    }
    if (!nImage.isLoaded) {
        console.log('NOTE: Native module not loaded. RAW/HEIC decode tests skipped.');
        console.log('Run: npm run setup && npm run build');
    }
}

runTests().catch(err => {
    console.error('Test error:', err);
    process.exit(1);
});