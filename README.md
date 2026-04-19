# DSX Drumb - C++ Desktop Application

![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)
![Build](https://img.shields.io/badge/Build-CMake-success)

## Description
DSX Drumb is a lightweight, natively compiled Windows application designed to perform as an ultra-low latency drum machine and pad sampler. The program simulates hot-swappable external hardware controllers directly on a conventional keyboard grid, executing completely async sound buffers simultaneously without stuttering or crashing.

### The Philosophy: C/C++ Desktop over Web Frameworks
In an era dominated by large Electron deployments, CEF, and heavy cross-platform frameworks, DSX Drumb holds true to the **"C++ creates smaller, faster applications"** philosophy. 
By utilizing pure Win32 GDI routines for rendering and direct Microsoft XAudio2 hardware interfaces for sound processing, this software completely eliminates virtual machine bloat, gigabytes of RAM dependence, and background garbage-collection latency. The result is a robust `.exe` binary that launches seamlessly in milliseconds while retaining strict memory efficiency suited precisely for real-time musical applications.

## Technical Specifications
* **Architecture**: Natively drawn Windows GDI Event loop without reliance on large external UI libraries.
* **Audio API**: `XAudio2` configured for asynchronous multi-voice pooling (over 4 overlapping instances per pad).
* **Presets Management**: Layer configurations and keyboard-to-file maps actively synchronized via static lightweight `nlohmann/json`. 
* **Custom Config Tracking**: Direct integration with hot-swapping pads using pure native `GetOpenFileName` logic.

# Video Documentation

https://github.com/user-attachments/assets/9918f9d2-7c5b-424e-8572-f13b9aaa0b91

# Picture Of DSX Rakitan Ashooy

<img width="1600" height="1063" alt="WhatsApp Image 2026-04-20 at 01 53 38" src="https://github.com/user-attachments/assets/540ae889-5f2e-4a5d-bc99-ddafccdcccf6" />






## System Requirements
- **Operating System**: Windows 10/11
- **Tools**: Visual Studio 2022 (MSVC Toolchain), Windows 10 SDK (for XAudio2 support)
- **CMake**: >= 3.20

## Installation Details
There are no global installations required! Simply copy the standalone built executable alongside a folder titled `soundpack`. 

    ├── DSX_CSoundApp.exe
    └── soundpack/
        ├── layer.conf
        ├── preset1/
        │   ├── config.json
        │   └── kicks.wav
        └── preset2/

## Building Locally
Ensure you have CMake correctly placed in your environment variables and Microsoft Visual Studio 2022 installed.

1. Clone or clone this repository anywhere on your PC.
2. Initialize build directory and configure generators:
   `cmake -B build -G "Visual Studio 17 2022" -A x64`
3. Hit compile:
   `cmake --build build --config Release`
4. Jump into `./build/Release/` and run your `DSX_CSoundApp.exe`. 
