/**
 * nImage NAPI Bindings
 *
 * Provides JavaScript interface to native image decoding
 */

#include <napi.h>
#include "decoder.h"
#include <memory>
#include <vector>

/**
 * Convert ImageData to JavaScript object
 */
static Napi::Object ImageDataToJS(Napi::Env env, const ImageData& data) {
    Napi::Object obj = Napi::Object::New(env);

    obj.Set("width", Napi::Number::New(env, data.width));
    obj.Set("height", Napi::Number::New(env, data.height));
    obj.Set("bitsPerChannel", Napi::Number::New(env, data.bitsPerChannel));
    obj.Set("channels", Napi::Number::New(env, data.channels));
    obj.Set("colorSpace", Napi::String::New(env, data.colorSpace));
    obj.Set("format", Napi::String::New(env, data.format));
    obj.Set("hasAlpha", Napi::Boolean::New(env, data.hasAlpha));

    // Pixel data as Buffer
    if (!data.data.empty()) {
        Napi::Buffer<uint8_t> dataBuf = Napi::Buffer<uint8_t>::Copy(
            env, data.data.data(), data.data.size());
        obj.Set("data", dataBuf);
    } else {
        obj.Set("data", env.Null());
    }

    // ICC profile
    if (!data.iccProfile.empty()) {
        Napi::Buffer<uint8_t> iccBuf = Napi::Buffer<uint8_t>::Copy(
            env, data.iccProfile.data(), data.iccProfile.size());
        obj.Set("iccProfile", iccBuf);
    } else {
        obj.Set("iccProfile", env.Null());
    }

    // Camera metadata
    Napi::Object camera = Napi::Object::New(env);
    camera.Set("make", Napi::String::New(env, data.cameraMake));
    camera.Set("model", Napi::String::New(env, data.cameraModel));
    obj.Set("camera", camera);

    // Capture metadata
    Napi::Object capture = Napi::Object::New(env);
    capture.Set("dateTime", Napi::String::New(env, data.dateTime));
    capture.Set("exposureTime", Napi::Number::New(env, data.exposureTime));
    capture.Set("fNumber", Napi::Number::New(env, data.fNumber));
    capture.Set("isoSpeed", Napi::Number::New(env, data.isoSpeed));
    capture.Set("focalLength", Napi::Number::New(env, data.focalLength));
    obj.Set("capture", capture);

    // Raw dimensions
    Napi::Object raw = Napi::Object::New(env);
    raw.Set("width", Napi::Number::New(env, data.rawWidth));
    raw.Set("height", Napi::Number::New(env, data.rawHeight));
    obj.Set("raw", raw);

    obj.Set("orientation", Napi::Number::New(env, data.orientation));

    return obj;
}

/**
 * Convert ImageMetadata to JavaScript object
 */
static Napi::Object MetadataToJS(Napi::Env env, const ImageMetadata& meta) {
    Napi::Object obj = Napi::Object::New(env);

    obj.Set("format", Napi::String::New(env, meta.format));
    obj.Set("width", Napi::Number::New(env, meta.width));
    obj.Set("height", Napi::Number::New(env, meta.height));
    obj.Set("rawWidth", Napi::Number::New(env, meta.rawWidth));
    obj.Set("rawHeight", Napi::Number::New(env, meta.rawHeight));
    obj.Set("hasAlpha", Napi::Boolean::New(env, meta.hasAlpha));
    obj.Set("bitsPerSample", Napi::Number::New(env, meta.bitsPerSample));
    obj.Set("orientation", Napi::Number::New(env, meta.orientation));
    obj.Set("fileSize", Napi::Number::New(env, static_cast<double>(meta.fileSize)));

    // Camera
    Napi::Object camera = Napi::Object::New(env);
    camera.Set("make", Napi::String::New(env, meta.cameraMake));
    camera.Set("model", Napi::String::New(env, meta.cameraModel));
    obj.Set("camera", camera);

    // Capture settings
    Napi::Object capture = Napi::Object::New(env);
    capture.Set("exposureTime", Napi::Number::New(env, meta.exposureTime));
    capture.Set("fNumber", Napi::Number::New(env, meta.fNumber));
    capture.Set("isoSpeed", Napi::Number::New(env, meta.isoSpeed));
    capture.Set("focalLength", Napi::Number::New(env, meta.focalLength));
    capture.Set("lensModel", Napi::String::New(env, meta.lensModel));
    obj.Set("capture", capture);

    obj.Set("colorSpace", Napi::String::New(env, meta.colorSpace));

    return obj;
}

/**
 * Convert FormatDetection to JavaScript object
 */
static Napi::Object FormatDetectionToJS(Napi::Env env, const FormatDetection& det) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("format", Napi::String::New(env, ImageFormatUtil::formatToString(det.format)));
    obj.Set("confidence", Napi::Number::New(env, det.confidence));
    obj.Set("mimeType", Napi::String::New(env, det.mimeType));
    return obj;
}

// ============================================================================
// NAPI Wrapper Classes
// ============================================================================

/**
 * ImageDecoder wrapper - wraps the ImageDecoder base class
 */
class ImageDecoderWrapper : public Napi::ObjectWrap<ImageDecoderWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    ImageDecoderWrapper(const Napi::CallbackInfo& info);
    ~ImageDecoderWrapper();

private:
    std::unique_ptr<ImageDecoder> decoder_;
    ImageFormat forcedFormat_;

    Napi::Value Decode(const Napi::CallbackInfo& info);
    Napi::Value GetMetadata(const Napi::CallbackInfo& info);
    Napi::Value GetError(const Napi::CallbackInfo& info);
};

ImageDecoderWrapper::ImageDecoderWrapper(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ImageDecoderWrapper>(info)
    , forcedFormat_(ImageFormat::UNKNOWN) {

    // Check if format was specified
    if (info.Length() >= 1 && info[0].IsString()) {
        std::string formatStr = info[0].As<Napi::String>().Utf8Value();
        forcedFormat_ = ImageFormatUtil::formatFromString(formatStr);
    }

    // Create appropriate decoder
    if (forcedFormat_ != ImageFormat::UNKNOWN) {
        decoder_ = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoder(forcedFormat_));
    }
    // Otherwise, decoder will be created on first decode based on format detection
}

ImageDecoderWrapper::~ImageDecoderWrapper() {
    // Unique_ptr handles cleanup
}

Napi::Object ImageDecoderWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "ImageDecoder", {
        InstanceMethod("decode", &ImageDecoderWrapper::Decode),
        InstanceMethod("getMetadata", &ImageDecoderWrapper::GetMetadata),
        InstanceMethod("getError", &ImageDecoderWrapper::GetError),
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("ImageDecoder", func);
    return exports;
}

Napi::Value ImageDecoderWrapper::Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer as first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint8_t* data = inputBuffer.Data();
    size_t size = inputBuffer.Length();

    // Auto-detect format if not specified at construction
    if (!decoder_) {
        FormatDetection detected = ImageDecoder::detectFormat(data, size);
        if (detected.format == ImageFormat::UNKNOWN) {
            Napi::Error::New(env, "Unable to detect image format").ThrowAsJavaScriptException();
            return env.Null();
        }
        decoder_ = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoder(detected.format));
        if (!decoder_) {
            Napi::Error::New(env, "No decoder available for detected format: " +
                           ImageFormatUtil::formatToString(detected.format)).ThrowAsJavaScriptException();
            return env.Null();
        }
    }

    ImageData output;
    bool success = decoder_->decode(data, size, output);

    if (!success) {
        Napi::Error::New(env, "Decode failed: " + decoder_->getError()).ThrowAsJavaScriptException();
        return env.Null();
    }

    return ImageDataToJS(env, output);
}

Napi::Value ImageDecoderWrapper::GetMetadata(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer as first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint8_t* data = inputBuffer.Data();
    size_t size = inputBuffer.Length();

    // Auto-detect format if decoder not yet created
    if (!decoder_) {
        FormatDetection detected = ImageDecoder::detectFormat(data, size);
        if (detected.format == ImageFormat::UNKNOWN) {
            Napi::Error::New(env, "Unable to detect image format").ThrowAsJavaScriptException();
            return env.Null();
        }
        decoder_ = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoder(detected.format));
    }

    ImageMetadata meta;
    bool success = decoder_->getMetadata(data, size, meta);

    if (!success) {
        Napi::Error::New(env, "getMetadata failed: " + decoder_->getError()).ThrowAsJavaScriptException();
        return env.Null();
    }

    return MetadataToJS(env, meta);
}

Napi::Value ImageDecoderWrapper::GetError(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (decoder_) {
        return Napi::String::New(env, decoder_->getError());
    }
    return Napi::String::New(env, "");
}

// ============================================================================
// Standalone Functions
// ============================================================================

/**
 * Detect format from buffer
 */
static Napi::Value DetectFormat(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer as first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint8_t* data = inputBuffer.Data();
    size_t size = inputBuffer.Length();

    FormatDetection detected = ImageDecoder::detectFormat(data, size);
    return FormatDetectionToJS(env, detected);
}

/**
 * Get supported formats
 */
static Napi::Value GetSupportedFormats(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    Napi::Array all = Napi::Array::New(env);
    size_t idx = 0;

    // RAW formats (LibRaw)
    const char* rawFormats[] = {"cr2", "nef", "arw", "orf", "raf", "rw2", "dng", "pef", "srw", "rwl", "crw", "mrw", "nrw", "erf", "3fr", "k25", "kdc", "mef", "mos", "mraw", "rrf", "sr2", "rwz"};
    for (const auto& f : rawFormats) {
        all.Set(idx++, Napi::String::New(env, f));
    }

    // HEIC/AVIF (LibHeif)
    all.Set(idx++, Napi::String::New(env, "heic"));
    all.Set(idx++, Napi::String::New(env, "heif"));
    all.Set(idx++, Napi::String::New(env, "avif"));

    // Standard formats (Sharp)
    const char* standardFormats[] = {"jpeg", "jpg", "png", "tiff", "tif", "webp", "gif", "bmp", "jxl"};
    for (const auto& f : standardFormats) {
        all.Set(idx++, Napi::String::New(env, f));
    }

    // Document formats (ImageMagick)
    const char* documentFormats[] = {"psd", "pdf", "svg", "ai", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "eps", "xps"};
    for (const auto& f : documentFormats) {
        all.Set(idx++, Napi::String::New(env, f));
    }

    // Scientific/imaging formats (ImageMagick)
    const char* scientificFormats[] = {"exr", "hdr", "bigtiff", "cin", "dpx", "fits", "flif", "j2k", "jp2", "miff", "mpc", "pcd", "pfm", "pict", "ppm", "psb", "psp", "sgi", "tga", "vtf"};
    for (const auto& f : scientificFormats) {
        all.Set(idx++, Napi::String::New(env, f));
    }

    // Other ImageMagick formats
    const char* otherFormats[] = {
        "bmp2", "bmp3", "dib", "dds", "djvu", "svgz", "wmf", "emf", "cur", "ico",
        "pcx", "dcx", "sun", "ras", "pbm", "pgm", "pnm", "xbm", "xpm", "xwd", "wbmp",
        "avi", "mpg", "mpeg", "mov", "mp4", "m4v", "wmv", "flv", "mkv", "mng", "jng", "mpo",
        "gray", "graya", "cmyk", "cmyka", "rgb", "rgba", "rgb565", "rgba555",
        "aai", "art", "blp", "brf", "cals", "caption", "clip", "dcm", "dot", "dpr",
        "font", "fpx", "fractal", "g", "g3", "g4", "gradient", "grbo", "h", "hald",
        "hcl", "histogram", "hrz", "htm", "html", "icb", "icc", "icer", "info",
        "inline", "ipl", "isobrl", "isobrl6", "jbg", "jnx", "jpe", "jpm", "jps", "jpx",
        "k", "kvec", "label", "linux", "mac", "map", "mat", "matte", "mono", "msl",
        "mtv", "mvg", "null", "ora", "otb", "pal", "palm", "pcds", "pdb", "pfa", "pfb",
        "picon", "pix", "pjpeg", "plasma", "png8", "png24", "png32", "pocketmod",
        "ps", "ps2", "ps3", "ptif", "pwp", "rgf", "rla", "rle", "rpm", "rsle",
        "sfw", "shtml", "stream", "text", "tile", "tim", "tm2", "ttc", "ttf", "txt",
        "ubrl", "ubrl6", "uil", "uyvy", "vda", "viff", "vips", "wdp", "wing", "wpg",
        "x", "xc", "xcf", "xv", "yuv", "yuva"
    };
    for (const auto& f : otherFormats) {
        all.Set(idx++, Napi::String::New(env, f));
    }

    return all;
}

/**
 * Decode an image directly (convenience function)
 */
static Napi::Value DecodeImage(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer as first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    // Optional format hint
    ImageFormat format = ImageFormat::UNKNOWN;
    if (info.Length() >= 2 && info[1].IsString()) {
        std::string formatStr = info[1].As<Napi::String>().Utf8Value();
        format = ImageFormatUtil::formatFromString(formatStr);
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint8_t* data = inputBuffer.Data();
    size_t size = inputBuffer.Length();

    // Create decoder
    std::unique_ptr<ImageDecoder> decoder;
    if (format != ImageFormat::UNKNOWN) {
        decoder = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoder(format));
    } else {
        decoder = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoderForBuffer(data, size));
    }

    if (!decoder) {
        Napi::Error::New(env, "Unable to create decoder for image").ThrowAsJavaScriptException();
        return env.Null();
    }

    ImageData output;
    bool success = decoder->decode(data, size, output);

    if (!success) {
        Napi::Error::New(env, "Decode failed: " + decoder->getError()).ThrowAsJavaScriptException();
        return env.Null();
    }

    return ImageDataToJS(env, output);
}

/**
 * Extract thumbnail from an image
 * Options:
 *   - size: max dimension (default 256)
 *   - format: optional format hint
 */
static Napi::Value GetThumbnail(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected Buffer as first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    int maxSize = 256;
    ImageFormat format = ImageFormat::UNKNOWN;

    if (info.Length() >= 2 && info[1].IsObject()) {
        Napi::Object options = info[1].As<Napi::Object>();
        if (options.Has("size") && options.Get("size").IsNumber()) {
            maxSize = options.Get("size").As<Napi::Number>().Int32Value();
        }
        if (options.Has("format") && options.Get("format").IsString()) {
            std::string formatStr = options.Get("format").As<Napi::String>().Utf8Value();
            format = ImageFormatUtil::formatFromString(formatStr);
        }
    }

    Napi::Buffer<uint8_t> inputBuffer = info[0].As<Napi::Buffer<uint8_t>>();
    uint8_t* data = inputBuffer.Data();
    size_t size = inputBuffer.Length();

    std::unique_ptr<ImageDecoder> decoder;
    if (format != ImageFormat::UNKNOWN) {
        decoder = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoder(format));
    } else {
        decoder = std::unique_ptr<ImageDecoder>(ImageDecoder::createDecoderForBuffer(data, size));
    }

    if (!decoder) {
        Napi::Error::New(env, "Unable to create decoder for thumbnail").ThrowAsJavaScriptException();
        return env.Null();
    }

    ImageData output;
    bool success = decoder->getThumbnail(data, size, maxSize, output);

    if (!success) {
        Napi::Error::New(env, "Thumbnail extraction failed: " + decoder->getError()).ThrowAsJavaScriptException();
        return env.Null();
    }

    return ImageDataToJS(env, output);
}

// ============================================================================
// Module Initialization
// ============================================================================

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    // Register the ImageDecoder class
    ImageDecoderWrapper::Init(env, exports);

    // Register standalone functions
    exports.Set("detectFormat", Napi::Function::New(env, DetectFormat));
    exports.Set("getSupportedFormats", Napi::Function::New(env, GetSupportedFormats));
    exports.Set("decode", Napi::Function::New(env, DecodeImage));
    exports.Set("thumbnail", Napi::Function::New(env, GetThumbnail));

    // Version info
    Napi::Object version = Napi::Object::New(env);
    version.Set("major", Napi::Number::New(env, 0));
    version.Set("minor", Napi::Number::New(env, 1));
    version.Set("patch", Napi::Number::New(env, 0));
    exports.Set("version", version);

    return exports;
}

NODE_API_MODULE(nimage, Init)
