#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const capabilities = {
    version: { major: 0, minor: 1, patch: 0 },
    decoders: {
        raw: {
            library: 'libraw',
            formats: ['cr2', 'crw', 'nef', 'nrw', 'arw', 'sr2', 'orf', 'raf', 'rw2', 'dng', 'pef', 'srw', 'rwl', 'mrw', 'erf', '3fr', 'k25', 'kdc', 'mef', 'mos', 'mraw', 'rrf', 'rwz'],
            features: ['halfSize', 'qualityPresets', 'thumbnails', 'metadata', 'streaming'],
            qualityLevels: [
                { level: 0, name: 'draft', algorithm: 'linear' },
                { level: 1, name: 'fast', algorithm: 'ppg' },
                { level: 2, name: 'balanced', algorithm: 'ahd' },
                { level: 3, name: 'best', algorithm: 'ahd+' }
            ]
        },
        heic: {
            library: 'libheif',
            formats: ['heic', 'heif', 'avif'],
            features: ['thumbnails', 'metadata', 'alphaChannel', 'qualityPresets'],
            threading: true,
            parallelTileDecode: true,
            hardwareAcceleration: false
        },
        sharp: {
            library: 'sharp/libvips',
            formats: ['jpeg', 'jpg', 'png', 'webp', 'tiff', 'tif', 'gif', 'bmp', 'jxl', 'jp2'],
            features: ['resize', 'crop', 'rotate', 'flip', 'flop', 'grayscale', 'negate', 'normalize', 'linear', 'extend', 'composite']
        },
        magick: {
            library: 'imagemagick',
            formats: [],
            features: ['150+ formats'],
            note: 'CLI fallback via magick.exe'
        }
    },
    encoders: { formats: ['jpeg', 'png', 'webp', 'avif', 'tiff'], library: 'sharp/libvips' },
    detection: { method: 'magic-bytes', minBytes: 12, speed: '~0.5µs' },
    stats: { totalFormats: 212 },
    generatedAt: new Date().toISOString()
};

// Try to get actual format list from native module
try {
    const nImage = require('..');
    if (nImage.isLoaded && nImage.getSupportedFormats) {
        const formats = nImage.getSupportedFormats();
        capabilities.stats.totalFormats = formats.length;
    }
} catch (e) {
    // Native module not available, use static data
}

const capsDir = path.join(__dirname, '..', 'lib', 'capabilities');
if (!fs.existsSync(capsDir)) fs.mkdirSync(capsDir, { recursive: true });

const outputPath = path.join(capsDir, 'index.json');
fs.writeFileSync(outputPath, JSON.stringify(capabilities, null, 2));
console.log(`Capabilities written to ${outputPath}`);
console.log(`Total formats: ${capabilities.stats.totalFormats}`);
