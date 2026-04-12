#include "WavLoader.h"
#include <fstream>
#include <iostream>

#define FOURCC(c0, c1, c2, c3) ( (DWORD)(BYTE)(c0) | ((DWORD)(BYTE)(c1) << 8) | \
                                ((DWORD)(BYTE)(c2) << 16) | ((DWORD)(BYTE)(c3) << 24) )

WavData WavLoader::LoadWavFile(const std::string& filePath) {
    WavData data;
    ZeroMemory(&data.wfx, sizeof(data.wfx));

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return data;

    DWORD chunkId, chunkSize;

    // Read RIFF header
    file.read(reinterpret_cast<char*>(&chunkId), sizeof(DWORD));
    file.read(reinterpret_cast<char*>(&chunkSize), sizeof(DWORD));
    if (chunkId != FOURCC('R', 'I', 'F', 'F')) return data;

    DWORD format;
    file.read(reinterpret_cast<char*>(&format), sizeof(DWORD));
    if (format != FOURCC('W', 'A', 'V', 'E')) return data;

    bool foundFmt = false, foundData = false;

    while (file.read(reinterpret_cast<char*>(&chunkId), sizeof(DWORD))) {
        file.read(reinterpret_cast<char*>(&chunkSize), sizeof(DWORD));

        if (chunkId == FOURCC('f', 'm', 't', ' ')) {
            file.read(reinterpret_cast<char*>(&data.wfx), min(chunkSize, (DWORD)sizeof(WAVEFORMATEX)));
            if (chunkSize > sizeof(WAVEFORMATEX)) {
                file.seekg(chunkSize - sizeof(WAVEFORMATEX), std::ios::cur);
            }
            foundFmt = true;
        }
        else if (chunkId == FOURCC('d', 'a', 't', 'a')) {
            data.audioData.resize(chunkSize);
            file.read(reinterpret_cast<char*>(data.audioData.data()), chunkSize);
            foundData = true;
            break; // Stop after data chunk
        }
        else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (foundFmt && foundData) {
        data.isValid = true;
    }

    return data;
}
