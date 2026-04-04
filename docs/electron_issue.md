# Integration Handover: nImage & Electron

## Background
We attempted to integrate the custom N-API native image converter (`nImage`) into an Electron application (`Electron Slide`). The module works perfectly in pure Node.js (via CLI or `spawn`), but requiring it directly inside the Electron frontend or main process causes an instant, fatal crash (Exit Code 1 / 0xC0000005 Access Violation).

## The Problem: DLL Collision (DLL Hell)
The root cause is a dynamic linking collision. Electron (v41) pre-loads a massive, customized Chromium engine containing its own versions of common graphics and utility DLLs (like `vulkan-1.dll`, `zlib.dll`, `libjpeg.dll`, `libpng.dll`).

When Windows `LoadLibrary` attempts to load our dynamically linked N-API module, it sees that these DLLs are already loaded into the process memory space. Instead of loading `nImage`'s shipped DLL versions, the OS dynamically links `nImage` to Electron's in-memory Chromium DLLs. This creates a severe ABI/API mismatch, leading to the immediate crash.

## Rejected Fallback: IPC Worker
We successfully ran `nImage` out-of-process via `child_process.spawn()` during testing. However, this negates the primary benefit of building a native N-API module. An out-of-process IPC approach limits performance by stripping away our ability to use zero-copy memory pointer sharing, making it no more efficient than standard CLI execution (such as the existing `magick.exe` implementation).

## The Solution: Selective Static Compilation & CLI Fallback
To retain native zero-copy performance inside Electron without a monolithic 150MB+ binary, the native N-API module now bypasses the OS DLL search order for problematic libraries (like `MagickCore`).

The path forward implemented was **Selective Static Compilation**:
1. We statically compiled the core performance codecs (`libraw` and `libheif`) into the `nImage.node` binary so they have zero external DLL dependencies for their core business logic.
2. Minor dynamic dependencies (`libde265`, `libaom`) were kept to minimal, non-conflicting DLLs.
3. The massively conflicting `ImageMagick` (which uses 50+ custom graphics DLLs) was completely removed from the N-API C++ extension.
4. For edge-case formats that `sharp`, `libraw`, and `libheif` cannot handle natively (like TGA, PSD, EXR), the Node.js wrapper (`lib/index.js`) seamlessly falls back to spawning the `magick.exe` CLI process. 

This hybrid `.node` file + CLI fallback is self-contained and performant for 99% of images, while completely avoiding collision with Electron's pre-loaded dependencies, securing both stability and performance.