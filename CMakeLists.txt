cmake_minimum_required(VERSION 3.20)
project(DSX_CSoundApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set Windows subsystem to GUI so we don't get a command prompt behind the UI
set(CMAKE_WIN32_EXECUTABLE True)

# Source files
set(SOURCES
    main.cpp
    WavLoader.cpp
    AudioEngine.cpp
    PresetManager.cpp
)

# Headers
set(HEADERS
    WavLoader.h
    AudioEngine.h
    PresetManager.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Include local libraries folder for json.hpp
target_include_directories(${PROJECT_NAME} PRIVATE libs)

# Link dependencies
# XAudio2 is standard in Windows SDK
target_link_libraries(${PROJECT_NAME} PRIVATE xaudio2.lib)
