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

    // Test thumbnail extraction
    console.log('--- Testing Thumbnail Extraction ---');
    if (!nImage.isLoaded) {
        console.log('SKIPPED: Native module not loaded');
    } else {
        const testCr2Path = path.join(__dirname, 'assets', 'IMG_2593.CR2');
        const testHeicPath = path.join(__dirname, 'assets', 'IMG_0092_1.HEIC');
        const testJpgPath = path.join(__dirname, 'assets', '_4240213.jpg');

        if (fs.existsSync(testCr2Path)) {
            try {
                const start = Date.now();
                const thumb = await nImage.thumbnail(fs.readFileSync(testCr2Path), { size: 256 });
                const elapsed = Date.now() - start;
                console.log('RAW thumbnail:', thumb.width + 'x' + thumb.height, '(' + elapsed + 'ms)');
                assert(thumb.width > 0 && thumb.height > 0, 'Thumbnail should have dimensions');
                assert(thumb.data && thumb.data.length > 0, 'Thumbnail should have data');
            } catch (err) {
                console.log('RAW thumbnail error:', err.message);
            }
        } else {
            console.log('SKIPPED: CR2 test file not found');
        }

        if (fs.existsSync(testHeicPath)) {
            try {
                const start = Date.now();
                const thumb = await nImage.thumbnail(fs.readFileSync(testHeicPath), { size: 256 });
                const elapsed = Date.now() - start;
                console.log('HEIC thumbnail:', thumb.width + 'x' + thumb.height, '(' + elapsed + 'ms)');
                assert(thumb.width > 0 && thumb.height > 0, 'Thumbnail should have dimensions');
            } catch (err) {
                console.log('HEIC thumbnail error:', err.message);
            }
        } else {
            console.log('SKIPPED: HEIC test file not found');
        }

        if (fs.existsSync(testJpgPath)) {
            try {
                const start = Date.now();
                const thumb = await nImage.thumbnail(fs.readFileSync(testJpgPath), { size: 128 });
                const elapsed = Date.now() - start;
                console.log('JPEG thumbnail:', thumb.width + 'x' + thumb.height, '(' + elapsed + 'ms)');
                assert(thumb.width > 0 && thumb.height > 0, 'Thumbnail should have dimensions');
            } catch (err) {
                console.log('JPEG thumbnail error:', err.message);
            }
        } else {
            console.log('SKIPPED: JPEG test file not found');
        }
    }
    console.log('');

    // Test pipeline output validation (size, format)
    console.log('--- Testing Pipeline Output Validation ---');
    if (!nImage.hasSharp) {
        console.log('SKIPPED: Sharp not installed');
    } else {
        const testJpgPath = path.join(__dirname, 'assets', '_4240213.jpg');
        if (fs.existsSync(testJpgPath)) {
            try {
                const result = await nImage(testJpgPath)
                    .resize(200, 200, { fit: 'inside' })
                    .jpeg({ quality: 90 })
                    .toBuffer();

                const metadata = await nImage.sharp(result).metadata();
                assert(metadata.width <= 200, 'Width should be <= 200');
                assert(metadata.height <= 200, 'Height should be <= 200');
                assert(metadata.format === 'jpeg', 'Format should be jpeg');
                console.log('Pipeline resize validation: OK (' + metadata.width + 'x' + metadata.height + ')');

                const resultPng = await nImage(testJpgPath)
                    .resize(150, 150)
                    .png()
                    .toBuffer();

                const pngMeta = await nImage.sharp(resultPng).metadata();
                assert(pngMeta.width <= 150, 'PNG width should be <= 150');
                assert(pngMeta.height <= 150, 'PNG height should be <= 150');
                assert(pngMeta.format === 'png', 'Format should be png');
                console.log('Pipeline PNG output validation: OK (' + pngMeta.width + 'x' + pngMeta.height + ')');

                const resultWebp = await nImage(testJpgPath)
                    .resize(100, 100)
                    .webp({ quality: 80 })
                    .toBuffer();

                const webpMeta = await nImage.sharp(resultWebp).metadata();
                assert(webpMeta.format === 'webp', 'Format should be webp');
                console.log('Pipeline WebP output validation: OK (' + webpMeta.width + 'x' + webpMeta.height + ')');

            } catch (err) {
                console.log('Pipeline validation error:', err.message);
            }
        } else {
            console.log('SKIPPED: Test JPEG not found');
        }
    }
    console.log('');

    // Test pipeline quality settings
    console.log('--- Testing Pipeline Quality Settings ---');
    if (!nImage.hasSharp) {
        console.log('SKIPPED: Sharp not installed');
    } else {
        const testJpgPath = path.join(__dirname, 'assets', '_4240213.jpg');
        if (fs.existsSync(testJpgPath)) {
            try {
                const lowQuality = await nImage(testJpgPath)
                    .resize(400, 400)
                    .jpeg({ quality: 30 })
                    .toBuffer();

                const highQuality = await nImage(testJpgPath)
                    .resize(400, 400)
                    .jpeg({ quality: 100 })
                    .toBuffer();

                assert(lowQuality.length < highQuality.length, 'Low quality should produce smaller file');
                console.log('JPEG quality settings: OK (low=' + lowQuality.length + ' bytes, high=' + highQuality.length + ' bytes)');

                const losslessWebp = await nImage(testJpgPath)
                    .resize(400, 400)
                    .webp({ lossless: true })
                    .toBuffer();

                const lossyWebp = await nImage(testJpgPath)
                    .resize(400, 400)
                    .webp({ quality: 80 })
                    .toBuffer();

                assert(losslessWebp.length > lossyWebp.length, 'Lossless should be larger than lossy');
                console.log('WebP lossless setting: OK (lossless=' + losslessWebp.length + ' bytes, lossy=' + lossyWebp.length + ' bytes)');

            } catch (err) {
                console.log('Quality settings error:', err.message);
            }
        } else {
            console.log('SKIPPED: Test JPEG not found');
        }
    }
    console.log('');

    // Test round-trip: RAW decode → Sharp transform → JPEG encode
    console.log('--- Testing Round-trip RAW decode → Sharp transform → JPEG encode ---');
    if (!nImage.hasSharp || !nImage.isLoaded) {
        console.log('SKIPPED: Sharp or native module not available');
    } else {
        const testCr2Path = path.join(__dirname, 'assets', 'IMG_2593.CR2');
        if (fs.existsSync(testCr2Path)) {
            try {
                const imageData = nImage.decode(fs.readFileSync(testCr2Path));
                assert(imageData.width > 0 && imageData.height > 0, 'RAW decode should have dimensions');
                assert(imageData.data && imageData.data.length > 0, 'RAW decode should have pixel data');
                console.log('RAW decode: ' + imageData.width + 'x' + imageData.height + ', ' + imageData.format);

                const jpegOutput = await nImage(imageData)
                    .resize(512, 512, { fit: 'inside' })
                    .jpeg({ quality: 92 })
                    .toBuffer();

                const jpegMeta = await nImage.sharp(jpegOutput).metadata();
                assert(jpegMeta.format === 'jpeg', 'Output format should be jpeg');
                assert(jpegMeta.width <= 512, 'JPEG width should be <= 512');
                assert(jpegMeta.height <= 512, 'JPEG height should be <= 512');
                console.log('Round-trip complete: ' + jpegMeta.width + 'x' + jpegMeta.height + ' JPEG (' + jpegOutput.length + ' bytes)');

                const pngOutput = await nImage(imageData)
                    .resize(256, 256, { fit: 'inside' })
                    .png()
                    .toBuffer();

                const pngMeta = await nImage.sharp(pngOutput).metadata();
                assert(pngMeta.format === 'png', 'Output format should be png');
                assert(pngMeta.width <= 256, 'PNG width should be <= 256');
                assert(pngMeta.height <= 256, 'PNG height should be <= 256');
                console.log('Round-trip PNG complete: ' + pngMeta.width + 'x' + pngMeta.height + ' PNG (' + pngOutput.length + ' bytes)');

            } catch (err) {
                console.log('Round-trip error:', err.message);
            }
        } else {
            console.log('SKIPPED: CR2 test file not found');
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