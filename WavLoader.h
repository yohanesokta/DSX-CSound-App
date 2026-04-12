#pragma once

#include <windows.h>
#include <vector>
#include <string>

struct WavData {
    WAVEFORMATEX wfx;
    std::vector<BYTE> audioData;
    bool isValid = false;
};

class WavLoader {
public:
    static WavData LoadWavFile(const std::string& filePath);
};
