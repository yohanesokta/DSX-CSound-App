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
                            if (element.value().is_string()) {
                                info.mapping[element.key()] = element.value().get<std::string>();
                            }
                        }
                        
                        presets.push_back(info);
                    } catch (const std::exception& e) {
                        std::cerr << "JSON parsing error for " << dirName << ": " << e.what() << std::endl;
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
    isEdited = false;
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
        if (mapping.find(logicalKey) != mapping.end() && !mapping.at(logicalKey).empty()) {
            return logicalKey; // sound id was registered using the logical key
        }
    }
    return "";
}

std::string PresetManager::GetFileNameForLogicalKey(const std::string& logicalKey) const {
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size()) {
        const auto& mapping = presets[currentPresetIndex].mapping;
        if (mapping.find(logicalKey) != mapping.end()) {
            return mapping.at(logicalKey);
        }
    }
    return "";
}

const std::string& PresetManager::GetCurrentPresetBasePath() const {
    static std::string empty = "";
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size()) {
        return presets[currentPresetIndex].basePath;
    }
    return empty;
}

bool PresetManager::IsCurrentPresetEdited() const {
    return isEdited;
}

void PresetManager::SetCurrentPresetEdited(bool edited) {
    isEdited = edited;
}

void PresetManager::UpdateMapping(const std::string& logicalKey, const std::string& wavFileName, AudioEngine& audioEngine) {
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size()) {
        auto& preset = presets[currentPresetIndex];
        preset.mapping[logicalKey] = wavFileName;
        
        std::string wavPath = preset.basePath + wavFileName;
        audioEngine.LoadSound(logicalKey, wavPath);
        
        isEdited = true;
    }
}

void PresetManager::SaveCurrentPreset() {
    if (currentPresetIndex >= 0 && currentPresetIndex < presets.size() && isEdited) {
        auto& preset = presets[currentPresetIndex];
        std::string configPath = preset.basePath + "config.json";
        
        std::ifstream inFile(configPath);
        json j;
        if (inFile.is_open()) {
            inFile >> j;
            inFile.close();
            for (const auto& pair : preset.mapping) {
                j["mapping"][pair.first] = pair.second;
            }
        } else {
            j["preset_name"] = preset.presetName;
            j["base_path"] = preset.basePath;
            for (const auto& pair : preset.mapping) {
                j["mapping"][pair.first] = pair.second;
            }
        }
        
        std::ofstream outFile(configPath);
        if (outFile.is_open()) {
            outFile << j.dump(4);
        }
        isEdited = false;
    }
}
