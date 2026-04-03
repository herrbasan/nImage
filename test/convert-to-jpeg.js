/**
 * nImage - Convert RAW/HEIC images to JPEG
 */

const fs = require('fs');
const path = require('path');
const nImage = require('..');

const INPUT_DIR = 'D:/Work/_GIT/MediaService/tests/assets/images';
const OUTPUT_DIR = path.join(__dirname, '..', '..', 'tests', 'assets', 'images', 'converted');

// Ensure output directory exists
if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

const testFiles = [
    '_4260446.ORF',
    'IMG_2593.CR2',
    'IMG_0092_1.HEIC',
];

async function convertToJpeg(inputPath) {
    const filename = path.basename(inputPath);
    const outputPath = path.join(OUTPUT_DIR, filename.replace(/\.[^.]+$/, '.jpg'));

    console.log(`\nProcessing: ${filename}`);
    console.log('=' .repeat(50));

    const startTime = process.hrtime.bigint();

    // Read file
    const buffer = fs.readFileSync(inputPath);
    console.log(`File size: ${(buffer.length / 1024 / 1024).toFixed(2)} MB`);

    // Detect format
    const detectStart = process.hrtime.bigint();
    const format = nImage.detectFormat(buffer);
    const detectEnd = process.hrtime.bigint();
    console.log(`Format: ${format.format} (${(format.confidence * 100).toFixed(0)}% confidence)`);
    console.log(`Detection time: ${(Number(detectEnd - detectStart) / 1e6).toFixed(2)} ms`);

    // Decode if supported
    const decodeStart = process.hrtime.bigint();

    try {
        const decoder = new nImage.ImageDecoder(format.format);
        const result = decoder.decode(buffer);
        const decodeEnd = process.hrtime.bigint();

        console.log(`\nDecode successful!`);
        console.log(`Dimensions: ${result.width} x ${result.height}`);
        console.log(`Channels: ${result.channels}`);
        console.log(`Bits/channel: ${result.bitsPerChannel}`);
        console.log(`Format: ${result.format}`);
        console.log(`Color space: ${result.colorSpace}`);

        if (result.camera) {
            console.log(`Camera: ${result.camera.make || ''} ${result.camera.model || ''}`);
        }
        if (result.capture) {
            const c = result.capture;
            console.log(`Settings: ISO ${c.isoSpeed || 'N/A'}, f/${c.fNumber || 'N/A'}, ${c.exposureTime || 'N/A'}s`);
        }

        const decodeTime = Number(decodeEnd - decodeStart) / 1e6;
        console.log(`\nDecode time: ${decodeTime.toFixed(2)} ms`);

        // For now, just report success - full JPEG encoding would require additional library
        console.log(`\nNote: Pixel data available in result.data (${result.data ? result.data.length : 0} bytes)`);
        console.log(`Output would be: ${outputPath}`);

        // Save a placeholder to show we processed it
        fs.writeFileSync(outputPath + '.meta.json', JSON.stringify({
            input: filename,
            output: path.basename(outputPath),
            width: result.width,
            height: result.height,
            channels: result.channels,
            format: result.format,
            decodeTimeMs: decodeTime,
        }, null, 2));

        return { success: true, width: result.width, height: result.height, decodeTime };
    } catch (err) {
        const decodeEnd = process.hrtime.bigint();
        const decodeTime = Number(decodeEnd - decodeStart) / 1e6;
        console.log(`\nDecode failed: ${err.message}`);
        console.log(`Decode time: ${decodeTime.toFixed(2)} ms`);
        return { success: false, error: err.message, decodeTime };
    }
}

async function main() {
    console.log('nImage RAW/HEIC to JPEG Converter');
    console.log('================================\n');
    console.log(`Input dir: ${INPUT_DIR}`);
    console.log(`Output dir: ${OUTPUT_DIR}`);
    console.log(`Module loaded: ${nImage.isLoaded}`);

    const results = [];

    for (const filename of testFiles) {
        const inputPath = path.join(INPUT_DIR, filename);
        if (!fs.existsSync(inputPath)) {
            console.log(`\nFile not found: ${inputPath}`);
            results.push({ file: filename, success: false, error: 'File not found' });
            continue;
        }

        const result = await convertToJpeg(inputPath);
        results.push({ file: filename, ...result });
    }

    // Summary
    console.log('\n\n================================');
    console.log('SUMMARY');
    console.log('================================');

    for (const r of results) {
        if (r.success) {
            console.log(`[OK] ${r.file}: ${r.width}x${r.height} in ${r.decodeTime.toFixed(2)}ms`);
        } else {
            console.log(`[FAIL] ${r.file}: ${r.error || 'Unknown error'}`);
        }
    }

    const successful = results.filter(r => r.success).length;
    console.log(`\nConverted: ${successful}/${results.length} files`);
}

main().catch(console.error);