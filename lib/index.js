/**
 * nImage - Native Image Processing for Node.js
 *
 * High-performance image processing via NAPI + Sharp
 * Supports: RAW formats (CR2, NEF, ARW, ORF, RAF, DNG), HEIC/HEIF, AVIF
 * All transformations and encoding via Sharp, RAW/HEIC decoding via native codecs.
 *
 * @example
 * const nImage = require('nimage');
 *
 * // Sharp-compatible pipeline (RAW/HEIC handled transparently)
 * const result = await nImage('photo.cr2')
 *     .resize(1024, 1024, { fit: 'inside' })
 *     .jpeg({ quality: 85 })
 *     .toBuffer();
 */

'use strict';

const path = require('path');
const fs = require('fs');

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
        // Try dist directory
        try {
            const distPath = path.join(__dirname, '..', 'dist', 'nimage.node');
            nativeBinding = require(distPath);
            loadError = null;
        } catch (e3) {
            // Module not built yet
        }
    }
}

// Track if sharp is available
let sharp = null;
try {
    sharp = require('sharp');
} catch (e) {
    // Sharp not available
}

// ============================================================================
// Format Detection (pure JS - no native dependency)
// ============================================================================

const RAW_FORMATS = new Set(['cr2', 'nef', 'arw', 'orf', 'raf', 'rw2', 'dng', 'pef', 'srw', 'rwl']);
const HEIC_FORMATS = new Set(['heic', 'heif', 'avif']);

/**
 * Detect image format from buffer (magic bytes)
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

    // Check TIFF/RAW signature (big-endian or little-endian)
    if ((buffer[0] === 0x4D && buffer[1] === 0x4D) || (buffer[0] === 0x49 && buffer[1] === 0x49)) {
        // Could be TIFF, CR2, NEF, ARW, DNG, etc.
        // TIFF-based RAW formats (CR2, NEF, ARW, ORF, RW2, DNG, etc.)
        // start with TIFF header (II or MM followed by 0x002A)
        // We can't easily distinguish them in pure JS, so treat as potential RAW/DNG
        if (buffer.length >= 4) {
            const isLittleEndian = buffer[0] === 0x49 && buffer[1] === 0x49;
            const isBigEndian = buffer[0] === 0x4D && buffer[1] === 0x4D;
            // Check for TIFF magic number (0x002A)
            const magic = isLittleEndian ? buffer[2] + (buffer[3] << 8) : (buffer[2] << 8) + buffer[3];
            if (magic === 0x002A) {
                // Check for specific RAW signatures at byte 8
                if (buffer.length >= 12) {
                    const sig = buffer.toString('ascii', 8, 11);
                    // Canon CR2
                    if (sig.startsWith('CR')) {
                        return { format: 'cr2', confidence: 0.9, mimeType: 'image/x-canon-cr2' };
                    }
                    // Nikon NEF
                    if (sig.startsWith('NE')) {
                        return { format: 'nef', confidence: 0.9, mimeType: 'image/x-nikon-nef' };
                    }
                    // Sony ARW
                    if (sig.startsWith('AR')) {
                        return { format: 'arw', confidence: 0.9, mimeType: 'image/x-sony-arw' };
                    }
                    // Olympus ORF (sometimes starts with OR)
                    if (sig.startsWith('OR')) {
                        return { format: 'orf', confidence: 0.9, mimeType: 'image/x-olympus-orf' };
                    }
                    // Fujifilm RAF
                    if (sig.startsWith('RA')) {
                        return { format: 'raf', confidence: 0.9, mimeType: 'image/x-fujifilm-raf' };
                    }
                    // Panasonic RW2
                    if (sig.startsWith('RW')) {
                        return { format: 'rw2', confidence: 0.9, mimeType: 'image/x-panasonic-rw2' };
                    }
                    // Adobe DNG
                    if (sig.startsWith('DNG')) {
                        return { format: 'dng', confidence: 0.9, mimeType: 'image/x-adobe-dng' };
                    }
                }
                // TIFF but no recognized RAW signature - could be standard TIFF or unknown RAW
                // Return as TIFF for now (Sharp can handle it directly)
                return { format: 'tiff', confidence: 0.95, mimeType: 'image/tiff' };
            }
        }
    }

    // Check WebP signature
    if (buffer[0] === 0x52 && buffer[1] === 0x49 && buffer[2] === 0x46 && buffer[3] === 0x46 &&
        buffer[8] === 0x57 && buffer[9] === 0x45 && buffer[10] === 0x42 && buffer[11] === 0x50) {
        return { format: 'webp', confidence: 0.95, mimeType: 'image/webp' };
    }

    // Check GIF signature
    if (buffer[0] === 0x47 && buffer[1] === 0x49 && buffer[2] === 0x46) {
        return { format: 'gif', confidence: 0.95, mimeType: 'image/gif' };
    }

    return { format: 'unknown', confidence: 0, mimeType: 'application/octet-stream' };
}

// ============================================================================
// Native Module Fallback Functions (when native module isn't built)
// ============================================================================

function decodeWithNative(buffer, formatHint) {
    if (!nativeBinding) {
        throw new Error('Native module not built. Run "npm run setup" and "npm run build".');
    }
    return nativeBinding.decode(buffer, formatHint);
}

class FallbackImageDecoder {
    constructor(format) {
        this.format = format;
    }

    decode(buffer) {
        if (!nativeBinding) {
            throw new Error('Native module not built. Run "npm run setup" and "npm run build".');
        }
        return nativeBinding.ImageDecoder ? new nativeBinding.ImageDecoder(this.format).decode(buffer) : decodeWithNative(buffer, this.format);
    }

    getMetadata(buffer) {
        if (!nativeBinding || !nativeBinding.ImageDecoder) {
            throw new Error('Native module not built. Run "npm run setup" and "npm run build".');
        }
        return new nativeBinding.ImageDecoder(this.format).getMetadata(buffer);
    }

    getError() {
        return nativeBinding ? '' : 'Native module not loaded';
    }
}

// ============================================================================
// Sharp Pipeline Wrapper
// ============================================================================

/**
 * nImage pipeline class - Sharp-compatible API with RAW/HEIC support
 */
class NImagePipeline {
    constructor(input, options = {}) {
        this._input = input;  // path, buffer, or ImageData
        this._options = options;
        this._sharp = null;   // Sharp instance
        this._operations = []; // chained operations
        this._outputFormat = null; // 'jpeg', 'png', 'webp', 'avif'
        this._outputOptions = {};
        this._formatDetected = null;
        this._rawBuffer = null;  // RAW image data if decoded
        this._width = null;
        this._height = null;
        this._channels = null;
    }

    /**
     * Initialize - detect format and decode RAW/HEIC if needed
     */
    async _init() {
        if (this._sharp) return; // already initialized

        let buffer;
        let formatDetection;

        // Get buffer from input
        if (Buffer.isBuffer(this._input)) {
            buffer = this._input;
        } else if (typeof this._input === 'string') {
            // File path
            buffer = fs.readFileSync(this._input);
        } else if (this._input && this._input.data && this._input.width && this._input.height) {
            // ImageData object from nImage.decode()
            this._rawBuffer = this._input;
            this._width = this._input.width;
            this._height = this._input.height;
            this._channels = this._input.channels;
            this._sharp = sharp(Buffer.from(this._input.data), {
                raw: {
                    width: this._input.width,
                    height: this._input.height,
                    channels: this._input.channels
                }
            });
            return;
        } else {
            throw new Error('Invalid input: must be a path, Buffer, or ImageData');
        }

        // Detect format
        formatDetection = detectFormat(buffer);
        this._formatDetected = formatDetection;

        // Check if format needs native decode
        const format = formatDetection.format;
        const needsNativeDecode = RAW_FORMATS.has(format) || HEIC_FORMATS.has(format);

        // Standard formats Sharp handles directly
        const standardFormats = new Set(['jpeg', 'png', 'webp', 'gif', 'tiff']);

        // If native module is loaded and format is not a standard Sharp format,
        // try native decode first (handles RAW and unusual formats)
        if (nativeBinding && !standardFormats.has(format)) {
            // Try native decode for RAW/HEIC/unknown formats
            if (needsNativeDecode || format === 'unknown') {
                try {
                    const imageData = nativeBinding.decode(buffer);
                    this._rawBuffer = imageData;
                    this._width = imageData.width;
                    this._height = imageData.height;
                    this._channels = imageData.channels;

                    // Create Sharp instance from decoded raw pixels
                    this._sharp = sharp(Buffer.from(imageData.data), {
                        raw: {
                            width: imageData.width,
                            height: imageData.height,
                            channels: imageData.channels
                        }
                    });
                    return;
                } catch (nativeErr) {
                    // Native decode failed, fall through to Sharp (if standard format)
                    if (standardFormats.has(format)) {
                        this._sharp = sharp(buffer);
                        return;
                    }
                    // For non-standard formats, re-throw the native error
                    throw nativeErr;
                }
            }
        }

        // Standard format - pass directly to Sharp
        this._sharp = sharp(buffer);
    }

    /**
     * Apply a Sharp operation
     */
    _addOperation(method, args) {
        this._operations.push({ method, args });
        return this;
    }

    /**
     * Execute all operations and return buffer
     */
    async _execute() {
        await this._init();

        // Apply operations
        for (const op of this._operations) {
            this._sharp = this._sharp[op.method](...op.args);
        }

        // Apply output format if specified
        if (this._outputFormat) {
            const formatMethod = this._outputFormat;
            this._sharp = this._sharp[formatMethod](this._outputOptions);
        }

        return this._sharp.toBuffer();
    }

    // =====================================================================
    // Sharp-Compatible Operations
    // =====================================================================

    resize(width, height, options) {
        return this._addOperation('resize', [width, height, options || {}]);
    }

    crop(options) {
        return this._addOperation('crop', [options || {}]);
    }

    crop(width, height, options) {
        if (typeof options === 'number') {
            // crop(width, height, [left, top])
            return this._addOperation('crop', [width, height, options]);
        }
        return this._addOperation('crop', [width, height]);
    }

    extract(options) {
        return this._addOperation('extract', [options]);
    }

    rotate(angle, options) {
        return this._addOperation('rotate', [angle, options || {}]);
    }

    flip(options) {
        return this._addOperation('flip', [options || {}]);
    }

    flop(options) {
        return this._addOperation('flop', [options || {}]);
    }

    grayscale(options) {
        return this._addOperation('grayscale', [options || {}]);
    }

    negate(options) {
        return this._addOperation('negate', [options || {}]);
    }

    normalize(options) {
        return this._addOperation('normalize', [options || {}]);
    }

    linear(alpha, beta, options) {
        return this._addOperation('linear', [alpha, beta, options || {}]);
    }

    reclip(options) {
        return this._addOperation('reclip', [options || {}]);
    }

    extend(options) {
        return this._addOperation('extend', [options]);
    }

    composite(overlay, options) {
        return this._addOperation('composite', [overlay, options || {}]);
    }

    // =====================================================================
    // Output Formats
    // =====================================================================

    jpeg(options = {}) {
        this._outputFormat = 'jpeg';
        this._outputOptions = options;
        return this;
    }

    png(options = {}) {
        this._outputFormat = 'png';
        this._outputOptions = options;
        return this;
    }

    webp(options = {}) {
        this._outputFormat = 'webp';
        this._outputOptions = options;
        return this;
    }

    avif(options = {}) {
        this._outputFormat = 'avif';
        this._outputOptions = options;
        return this;
    }

    tiff(options = {}) {
        this._outputFormat = 'tiff';
        this._outputOptions = options;
        return this;
    }

    // =====================================================================
    // Execution
    // =====================================================================

    /**
     * Execute pipeline and return buffer
     */
    async toBuffer() {
        return this._execute();
    }

    /**
     * Execute pipeline and write to file
     */
    async toFile(filePath) {
        const buffer = await this._execute();
        fs.writeFileSync(filePath, buffer);
        return this;
    }

    /**
     * Get metadata without full decode (if available)
     */
    async metadata() {
        await this._init();
        return this._sharp.metadata();
    }

    /**
     * Get raw pixel data
     */
    async raw() {
        await this._init();
        return this._sharp.raw().toBuffer();
    }

    /**
     * Get underlying Sharp instance (advanced use)
     */
    getSharp() {
        return this._sharp;
    }
}

// ============================================================================
// Main API
// ============================================================================

/**
 * Create a new pipeline with the given input
 * @param {string|Buffer|ImageData} input - File path, buffer, or ImageData
 * @returns {NImagePipeline}
 */
function nImage(input) {
    return new NImagePipeline(input);
}

// =====================================================================
// Native Module Functions (when available)
// =====================================================================

if (nativeBinding && !loadError) {
    const exportsObj = {
        // Sharp-compatible pipeline
        __nImage: nImage,

        // Core decoding
        decode: nativeBinding.decode,
        detectFormat,
        getSupportedFormats: nativeBinding.getSupportedFormats,

        // Class-based API
        ImageDecoder: nativeBinding.ImageDecoder,

        // Version info
        version: nativeBinding.version,

        // Flag to check if native module is loaded
        isLoaded: true,
        hasSharp: !!sharp,

        // Re-export sharp if available
        sharp: sharp,

        // Format utilities
        RAW_FORMATS: [...RAW_FORMATS],
        HEIC_FORMATS: [...HEIC_FORMATS],
    };

    // Copy all properties from exportsObj to nImage function
    Object.assign(nImage, exportsObj);
    module.exports = nImage;
} else {
    // =====================================================================
    // JavaScript Fallback (when native module isn't built)
    // =====================================================================

    const SUPPORTED_FORMATS = [
        'cr2', 'nef', 'arw', 'orf', 'raf', 'rw2', 'dng', 'pef', 'srw', 'rwl',
        'heic', 'heif', 'avif',
        'jpeg', 'png', 'webp', 'tiff', 'gif'
    ];

    const exportsObj = {
        // Sharp-compatible pipeline (requires sharp)
        __nImage: nImage,

        // Fallback decode (throws helpful error)
        decode(buffer, formatHint) {
            throw new Error(
                'nImage native module not built. ' +
                'Run "npm run setup" to download dependencies, ' +
                'then "npm run build" to compile.'
            );
        },
        detectFormat,
        getSupportedFormats: () => SUPPORTED_FORMATS,

        // Class-based API (placeholder when native module not built)
        ImageDecoder: FallbackImageDecoder,

        version: { major: 0, minor: 1, patch: 0 },
        isLoaded: false,
        hasSharp: !!sharp,
        sharp: sharp,
        loadError: loadError ? loadError.message : null,

        // Format utilities
        RAW_FORMATS: [...RAW_FORMATS],
        HEIC_FORMATS: [...HEIC_FORMATS],
    };

    // Copy all properties from exportsObj to nImage function
    Object.assign(nImage, exportsObj);
    module.exports = nImage;
}

// Alias for clarity
module.exports.NImagePipeline = NImagePipeline;