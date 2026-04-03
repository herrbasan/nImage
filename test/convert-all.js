/**
 * nImage Conversion Test
 * Tests all available file types with resize-by-half transformation
 */

const fs = require('fs');
const path = require('path');
const nImage = require('..');

const ASSETS_DIR = path.join(__dirname, 'assets');
const OUTPUT_DIR = path.join(__dirname, 'converted');

// Ensure output directory exists
if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

const testFiles = [
    { name: '_4240213.jpg', format: 'JPEG', expectedOutput: 'jpg' },
    { name: '_4250341.jpg', format: 'JPEG', expectedOutput: 'jpg' },
    { name: '116.png', format: 'PNG', expectedOutput: 'png' },
    { name: '117.png', format: 'PNG', expectedOutput: 'png' },
    { name: '118.png', format: 'PNG', expectedOutput: 'png' },
    { name: 'cutedance.gif', format: 'GIF', expectedOutput: 'gif' },
    { name: 'IMG_2593.CR2', format: 'CR2 (RAW)', expectedOutput: 'jpg' },
    { name: '_4260446.ORF', format: 'ORF (RAW)', expectedOutput: 'jpg' },
    { name: 'IMG_0092_1.HEIC', format: 'HEIC', expectedOutput: 'jpg' },
];

async function getImageSize(filePath) {
    try {
        const img = await nImage(filePath).resize(1, 1).toBuffer();
        // Get metadata from sharp
        const meta = await nImage(filePath).metadata();
        return { width: meta.width, height: meta.height };
    } catch (e) {
        return null;
    }
}

async function testOneFile(tf) {
    const inputPath = path.join(ASSETS_DIR, tf.name);
    const outputPath = path.join(OUTPUT_DIR, tf.name.replace(/\.[^.]+$/, '_half.jpg'));

    console.log(`\n--- Testing ${tf.format}: ${tf.name} ---`);

    if (!fs.existsSync(inputPath)) {
        console.log(`  SKIP: File not found`);
        return { name: testFile.name, format: testFile.format, status: 'SKIP', reason: 'File not found' };
    }

    try {
        // Get original size
        let originalSize = null;
        try {
            const meta = await nImage(inputPath).metadata();
            originalSize = { width: meta.width, height: meta.height };
            console.log(`  Original size: ${meta.width}x${meta.height}`);
        } catch (e) {
            console.log(`  Could not get original size: ${e.message}`);
        }

        // Perform resize by half and convert to JPEG
        const startTime = Date.now();
        const result = await nImage(inputPath)
            .resize(Math.floor((originalSize?.width || 1000) / 2), Math.floor((originalSize?.height || 1000) / 2), { fit: 'inside' })
            .jpeg({ quality: 85 })
            .toBuffer();
        const elapsed = Date.now() - startTime;

        // Verify output
        const isValidJpeg = result[0] === 0xFF && result[1] === 0xD8;
        if (!isValidJpeg) {
            throw new Error('Output is not a valid JPEG');
        }

        // Write output file
        fs.writeFileSync(outputPath, result);

        console.log(`  Output: ${result.length} bytes (${elapsed}ms)`);
        console.log(`  Output path: ${outputPath}`);
        console.log(`  Status: OK`);

        return { name: tf.name, format: tf.format, status: 'OK', size: result.length, time: elapsed };

    } catch (err) {
        console.log(`  ERROR: ${err.message}`);
        return { name: tf.name, format: tf.format, status: 'ERROR', error: err.message };
    }
}

async function runTests() {
    console.log('========================================');
    console.log('nImage Conversion Test - Resize by Half');
    console.log('========================================');
    console.log(`\nInput directory: ${ASSETS_DIR}`);
    console.log(`Output directory: ${OUTPUT_DIR}`);
    console.log(`\nTesting ${testFiles.length} file types...`);

    const results = [];

    for (const tf of testFiles) {
        const result = await testOneFile(tf);
        results.push(result);
    }

    // Summary
    console.log('\n========================================');
    console.log('Summary');
    console.log('========================================');

    const passed = results.filter(r => r.status === 'OK').length;
    const skipped = results.filter(r => r.status === 'SKIP').length;
    const failed = results.filter(r => r.status === 'ERROR').length;

    console.log(`\nPassed: ${passed}/${testFiles.length}`);
    console.log(`Skipped: ${skipped}`);
    console.log(`Failed: ${failed}`);

    if (failed > 0) {
        console.log('\nFailed files:');
        results.filter(r => r.status === 'ERROR').forEach(r => {
            console.log(`  - ${r.name}: ${r.error}`);
        });
    }

    console.log('\nOutput files written to:', OUTPUT_DIR);

    return failed === 0;
}

runTests()
    .then(success => {
        process.exit(success ? 0 : 1);
    })
    .catch(err => {
        console.error('Test error:', err);
        process.exit(1);
    });