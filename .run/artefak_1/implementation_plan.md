# DSX Drumb C++ Desktop Application Plan

This document outlines the implementation plan for the DSX Drumb-like desktop application requested, specifically built using C++, CMake, and standard Windows libraries (MSVC 2022).

## Goal
To build a professional, modular Windows Desktop application in C++ without a `src/` directory. The app acts as a drum machine, parsing `.wav` presets via JSON and interacting with keyboard layouts (Main and Numpad) for low-latency asynchronous audio playback. 

## Key Technologies
- **Build System**: CMake (Windows platform only).
- **Audio API**: **XAudio2** (Windows native part of Windows SDK) for professional, crash-free, and high-performance polyphonic audio without latency.
- **Window/UI**: Native Win32 API (`windows.h`). A simple graphical interface to display pad states and the active preset name. 
- **JSON Parsing**: `nlohmann/json` via CMake's `FetchContent` to parse `soundpack/presetX/config.json`.
- **Keyboard Handling**: `WM_KEYDOWN` / `WM_KEYUP` events to capture primary typing and numpad keys.

## User Review Required
> [!IMPORTANT]
> The UI will use the Native Win32 API (GDI drawing) rather than a heavy framework like Qt/PySide to maintain the "C++ pure Windows" approach without external UI binaries. Is this acceptable, or did you have another UI framework in mind like ImGui?
> XAudio2 is chosen as the "sound library" as it allows for asynchronous memory buffering out of the box and is the industry standard for Windows game audio, fulfilling your constraint `tanpa crash saat banyak play (async)`.

## Proposed Architecture / Changes

### Code Structure (Root Level Modularization)
- `CMakeLists.txt`: Fetches dependencies, links `XAudio2`, and configures the build for Visual Studio 2022.
- `main.cpp`: Entry point (`WinMain`), message loop, and App initialization.
- `Window.h` / `Window.cpp`: Handles Win32 Window registration, GDI drawing of the 3x5 pad grid, and propagating keyboard inputs.
- `AudioEngine.h` / `AudioEngine.cpp`: Manages `IXAudio2` instance, creating source voices, and mixing. Provides `PlaySoundAsync(buffer)` methods.
- `WavLoader.h` / `WavLoader.cpp`: RIFF chunk parser to read `.wav` into PCM byte buffers without memory leaks.
- `PresetManager.h` / `PresetManager.cpp`: Discovers `soundpack` folders, loads `config.json`, and coordinates which `.wav` buffers map to which keys.
- `App.h` / `App.cpp`: High-level controller connecting Keyboard inputs -> AudioEngine -> Window UI updates.

### Keyboard Mappings Config
| Logical Pad | Primary Key | Alt Key (Numpad) |
|---|---|---|
| (0,0) | `1` | `/` |
| (0,1) | `2` | `*` |
| (0,2) | `3` | `-` |
| (1,0) | `Q` | `7` |
| ... | ... | ... |
| (4,2) | `M` | `Enter` |

Up/Down arrows update the `PresetManager` index and reload wav files asynchronously or silently reload buffers.

## Open Questions
> [!WARNING]
> Since we'll map `Num(Enter)` and other specific Numpad keys, some keyboard configurations behave slightly differently with NumLock. We will process specific Virtual Key Codes (`VK_NUMPAD7`, `VK_DIVIDE`, etc.). Is that fine?

## Verification Plan
### Automated Tests
- CMake configuration test using `cmake -B build -G "Visual Studio 17 2022"`.
- Build executable using `cmake --build build --config Release`.
### Manual Verification
- Launch the application and observe the generated Window. 
- Press `Q`, `W`, `E` or `Numpad 7`, `8`, `9` and verify overlapping fast-playback doesn't crash or stutter. 
- Press `<UP>` / `<DOWN>` to switch between `/preset1/` and `/preset2/`.
