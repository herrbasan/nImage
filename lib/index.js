/**
 * nImage - Native Image Decoding for Node.js
 *
 * High-performance image decoding via NAPI
 * Supports: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG, etc.), HEIC/HEIF, AVIF
 *
 * @example
 * const nImage = require('nimage');
 *
 * // Decode an image buffer
 * const result = nImage.decode(imageBuffer);
 * console.log(result.width, result.height, result.format);
 *
 * // Get metadata without full decode
 * const meta = nImage.detectFormat(imageBuffer);
 * console.log(meta.format, meta.confidence);
 *
 * // Use the ImageDecoder class for multiple operations
 * const decoder = new nImage.ImageDecoder('raw');
 * const result = decoder.decode(imageBuffer);
 */

'use strict';

const path = require('path');

// Try to load the native module
let nativeBinding = null;
let loadError = null;

try {
    // Try build directory first (during development)
    const buildPath = path.join(__dirname, '..', 'build', 'Release', 'nimage.node');
    nativeBinding = require(buildPath);
} catch (e) {
    loadError = e;
    try {
        // Try prebuilds directory (installed package)
        const prebuildPath = path.join(__dirname, '..', 'prebuilds', 'nimage.node');
        nativeBinding = require(prebuildPath);
        loadError = null;
    } catch (e2) {
        // Module not built yet
    }
}

// ============================================================================
// JavaScript API (falls back gracefully when native module isn't built)
// ============================================================================

/**
 * @typedef {Object} ImageData
 * @property {number} width - Image width in pixels
 * @property {number} height - Image height in pixels
 * @property {number} bitsPerChannel - Bits per color channel
 * @property {number} channels - Number of channels (3=RGB, 4=RGBA)
 * @property {string} colorSpace - Color space (sRGB, AdobeRGB, etc.)
 * @property {string} format - Original format
 * @property {boolean} hasAlpha - Whether image has alpha channel
 * @property {Buffer|null} data - Pixel data
 * @property {Buffer|null} iccProfile - ICC color profile data
 * @property {Object} camera - Camera information
 * @property {string} camera.make - Camera manufacturer
 * @property {string} camera.model - Camera model
 * @property {Object} capture - Capture settings
 * @property {string} capture.dateTime - Date/time of capture
 * @property {number} capture.exposureTime - Exposure time in seconds
 * @property {number} capture.fNumber - F-number (aperture)
 * @property {number} capture.isoSpeed - ISO speed
 * @property {number} capture.focalLength - Focal length in mm
 * @property {Object} raw - Raw image dimensions
 * @property {number} raw.width - Sensor width
 * @property {number} raw.height - Sensor height
 * @property {number} orientation - EXIF orientation (1-8)
 */

/**
 * @typedef {Object} ImageMetadata
 * @property {string} format - Image format
 * @property {number} width - Image width
 * @property {number} height - Image height
 * @property {number} rawWidth - Raw sensor width
 * @property {number} rawHeight - Raw sensor height
 * @property {boolean} hasAlpha - Has alpha channel
 * @property {number} bitsPerSample - Bits per sample
 * @property {number} orientation - EXIF orientation
 * @property {number} fileSize - File size in bytes
 * @property {Object} camera - Camera info
 * @property {Object} capture - Capture settings
 * @property {string} colorSpace - Color space
 */

/**
 * @typedef {Object} FormatDetection
 * @property {string} format - Detected format (e.g., 'heic', 'cr2', 'jpeg')
 * @property {number} confidence - Confidence 0.0-1.0
 * @property {string} mimeType - MIME type
 */

// =====================================================================
// Native Module Functions (when available)
// =====================================================================

if (nativeBinding && !loadError) {
    module.exports = {
        // Core decoding
        decode: nativeBinding.decode,
        detectFormat: nativeBinding.detectFormat,
        getSupportedFormats: nativeBinding.getSupportedFormats,

        // Class-based API
        ImageDecoder: nativeBinding.ImageDecoder,

        // Version info
        version: nativeBinding.version,

        // Flag to check if native module is loaded
        isLoaded: true,
    };
} else {
    // =====================================================================
    // JavaScript Fallback (when native module isn't built)
    // =====================================================================

    const SUPPORTED_FORMATS = [
        'cr2', 'nef', 'arw', 'orf', 'raf', 'rw2', 'dng', 'pef', 'srw', 'rwl',
        'heic', 'heif', 'avif',
        'jpeg', 'png', 'webp', 'tiff', 'gif'
    ];

    /**
     * @param {Buffer} buffer
     * @returns {FormatDetection}
     */
    function detectFormat(buffer) {
        if (!Buffer.isBuffer(buffer) || buffer.length < 12) {
            return { format: 'unknown', confidence: 0, mimeType: 'application/octet-stream' };
        }

        // Check HEIC/HEIF signature (ftyp box)
        if (buffer[4] === 0x66 && buffer[5] === 0x74 && buffer[6] === 0x79 && buffer[7] === 0x70) {
            const brand = buffer.toString('ascii', 8, 12);
            if (brand.startsWith('heic')) {
                return { format: 'heic', confidence: 0.95, mimeType: 'image/heic' };
            }
            if (brand.startsWith('heif')) {
                return { format: 'heif', confidence: 0.95, mimeType: 'image/heif' };
            }
            if (brand.startsWith('avif')) {
                return { format: 'avif', confidence: 0.95, mimeType: 'image/avif' };
            }
        }

        // Check JPEG signature
        if (buffer[0] === 0xFF && buffer[1] === 0xD8 && buffer[2] === 0xFF) {
            return { format: 'jpeg', confidence: 0.95, mimeType: 'image/jpeg' };
        }

        // Check PNG signature
        if (buffer[0] === 0x89 && buffer[1] === 0x50 && buffer[2] === 0x4E && buffer[3] === 0x47) {
            return { format: 'png', confidence: 0.95, mimeType: 'image/png' };
        }

        // Check TIFF/RAW signature
        if ((buffer[0] === 0x4D && buffer[1] === 0x4D) || (buffer[0] === 0x49 && buffer[1] === 0x49)) {
            // Could be TIFF, CR2, NEF, ARW, DNG, etc.
            // For now, return as DNG (most common RAW from cameras)
            return { format: 'dng', confidence: 0.7, mimeType: 'image/x-adobe-dng' };
        }

        // Check WebP signature
        if (buffer[0] === 0x52 && buffer[1] === 0x49 && buffer[2] === 0x46 && buffer[3] === 0x46 &&
            buffer[8] === 0x57 && buffer[9] === 0x45 && buffer[10] === 0x42 && buffer[11] === 0x50) {
            return { format: 'webp', confidence: 0.95, mimeType: 'image/webp' };
        }

        return { format: 'unknown', confidence: 0, mimeType: 'application/octet-stream' };
    }

    /**
     * @param {Buffer} _buffer
     * @returns {ImageData}
     */
    function decode(_buffer) {
        throw new Error(
            'nImage native module not built. ' +
            'Run "npm run setup" to download dependencies, ' +
            'then "npm run build" to compile.'
        );
    }

    /**
     * ImageDecoder class (placeholder when native module not built)
     */
    class ImageDecoder {
        /**
         * @param {string} [_format] - Optional format hint
         */
        constructor(_format) {
            this.format = _format;
        }

        /**
         * @param {Buffer} _buffer
         * @returns {ImageData}
         */
        decode(_buffer) {
            throw new Error(
                'nImage native module not built. ' +
                'Run "npm run setup" to download dependencies, ' +
                'then "npm run build" to compile.'
            );
        }

        /**
         * @param {Buffer} _buffer
         * @returns {ImageMetadata}
         */
        getMetadata(_buffer) {
            throw new Error(
                'nImage native module not built. ' +
                'Run "npm run setup" to download dependencies, ' +
                'then "npm run build" to compile.'
            );
        }

        /**
         * @returns {string}
         */
        getError() {
            return 'Native module not loaded';
        }
    }

    module.exports = {
        decode,
        detectFormat,
        getSupportedFormats: () => SUPPORTED_FORMATS,
        ImageDecoder,
        version: { major: 0, minor: 1, patch: 0 },
        isLoaded: false,
        loadError: loadError ? loadError.message : null,
    };
}
