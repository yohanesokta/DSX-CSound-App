#include "PresetManager.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PresetManager::PresetManager() {}

bool PresetManager::DiscoverPresets(const std::string& rootPath) {
    presets.clear();
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((rootPath + "/*").c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string dirName = findData.cFileName;
            if (dirName != "." && dirName != "..") {
                std::string configPath = rootPath + "/" + dirName + "/config.json";
                std::ifstream configFile(configPath);
                
                if (configFile.is_open()) {
                    try {
                        json j;
                        configFile >> j;
                        
                        PresetInfo info;
                        info.presetName = j["preset_name"];
                        info.basePath = rootPath + "/" + dirName + "/";
                        
                        for (auto& element : j["mapping"].items()) {
                            info.mapping[element.key()] = element.value();
                        }
                        
                        presets.push_back(info);
                    } catch (...) {
                        // JSON parsing failed, skip
                    }
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    return !presets.empty();
}

bool PresetManager::LoadPreset(int index, AudioEngine& audioEngine) {
    if (index < 0 || index >= presets.size()) return false;
    
    audioEngine.UnloadAll();
    currentPresetIndex = index;
    const auto& preset = presets[index];
    
    for (const auto& pair : preset.mapping) {
        std::string wavPath = preset.basePath + pair.second;
        audioEngine.LoadSound(pair.first, wavPath);
    }
    
    return true;
}

const std::string& PresetManager::GetCurrentPresetName() const {
    static std::string empty = "";
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size()) {
        return presets[currentPresetIndex].presetName;
    }
    return empty;
}

std::string PresetManager::GetSoundIdForLogicalKey(const std::string& logicalKey) const {
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size()) {
        const auto& mapping = presets[currentPresetIndex].mapping;
        if (mapping.find(logicalKey) != mapping.end()) {
            return logicalKey; // sound id was registered using the logical key
        }
    }
    return "";
}
