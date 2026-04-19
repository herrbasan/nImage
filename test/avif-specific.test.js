const fs = require('fs');
const path = require('path');
const nImage = require('../lib/index'); // uses your local nImage wrapper

async function run() {
    try {
        console.log('--- Testing AVIF Encoding ---');
        const input = path.join(__dirname, 'assets', '_4250341.jpg');
        
        // 1. Test encoding: JPEG -> Sharp pipeline -> AVIF
        const avifBuffer = await nImage(input)
            .resize(200, 200)
            .avif({ quality: 50 })
            .toBuffer();
            
        console.log('SUCCESS: Encoded AVIF! Buffer length:', avifBuffer.length);
        const brand = avifBuffer.toString('ascii', 8, 12);
        console.log('Brand tag (magic bytes):', brand);
        
        if (brand === 'avif') {
            console.log('Magic bytes verified: Valid AVIF file.');
        } else {
            throw new Error('Not a valid AVIF.');
        }

        // 2. Test decoding: newly created AVIF -> nImage/libheif native decode -> JPEG
        console.log('\n--- Testing AVIF Decoding (via LibHeifDecoder) ---');
        const jpegBuffer = await nImage(avifBuffer)
            .jpeg()
            .toBuffer();

        console.log('SUCCESS: Decoded AVIF back to JPEG! Buffer length:', jpegBuffer.length);

        // Save to disk for inspection
        const outAvif = path.join(__dirname, 'converted', 'test_output.avif');
        const outJpeg = path.join(__dirname, 'converted', 'test_result.jpg');
        fs.writeFileSync(outAvif, avifBuffer);
        fs.writeFileSync(outJpeg, jpegBuffer);
        console.log(`\nFiles saved to disk for your inspection:`);
        console.log(`- ${outAvif}`);
        console.log(`- ${outJpeg}`);

    } catch (e) {
        console.error('Test failed:', e);
    }
}
run();
