# Integration Handover: nImage & Electron

## Background
We attempted to integrate the custom N-API native image converter (`nImage`) into an Electron application (`Electron Slide`). The module works perfectly in pure Node.js (via CLI or `spawn`), but requiring it directly inside the Electron frontend or main process causes an instant, fatal crash (Exit Code 1 / 0xC0000005 Access Violation).

## The Problem: DLL Collision (DLL Hell)
The root cause is a dynamic linking collision. Electron (v41) pre-loads a massive, customized Chromium engine containing its own versions of common graphics and utility DLLs (like `vulkan-1.dll`, `zlib.dll`, `libjpeg.dll`, `libpng.dll`).

When Windows `LoadLibrary` attempts to load our dynamically linked N-API module, it sees that these DLLs are already loaded into the process memory space. Instead of loading `nImage`'s shipped DLL versions, the OS dynamically links `nImage` to Electron's in-memory Chromium DLLs. This creates a severe ABI/API mismatch, leading to the immediate crash.

## Rejected Fallback: IPC Worker
We successfully ran `nImage` out-of-process via `child_process.spawn()` during testing. However, this negates the primary benefit of building a native N-API module. An out-of-process IPC approach limits performance by stripping away our ability to use zero-copy memory pointer sharing, making it no more efficient than standard CLI execution (such as the existing `magick.exe` implementation).

## The Solution: Static Compilation
To retain native zero-copy performance inside Electron, the native N-API module must bypass the OS DLL search order entirely to avoid loading conflicting libraries.

The path forward is to recompile the N-API module using **Static Compilation**. By linking all C++ dependencies (ImageMagick, libheif, libraw, Vulkan, etc.) using static libraries (`.lib` or `.a` via compiler flags like `/MT` in MSVC), all dependency machine code will be physically compiled directly into the final `nImage.node` binary. 

This monolithic `.node` file will be self-contained. Because it no longer relies on external dynamically linked `.dll` files for those libraries, it completely avoids collision with Electron's pre-loaded dependencies, securing both stability and performance.