#include "PresetManager.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

PresetManager::PresetManager() {}

bool PresetManager::DiscoverPresets(const std::string& rootPath) {
    presets.clear();
    currentPresetIndex = -1;
    
    // 1. Read layer.conf
    std::vector<std::string> layerOrder;
    std::string layerConfPath = rootPath + "/layer.conf";
    std::ifstream layerFile(layerConfPath);
    if (layerFile.is_open()) {
        std::string line;
        while (std::getline(layerFile, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back(); // handle CRLF
            if (!line.empty()) layerOrder.push_back(line);
        }
        layerFile.close();
    }
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((rootPath + "/*").c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::string dirName = findData.cFileName;
            if (dirName != "." && dirName != "..") {
                PresetInfo info;
                info.folderName = dirName;
                info.basePath = rootPath + "/" + dirName + "/";
                
                std::string configPath = info.basePath + "config.json";
                std::ifstream configFile(configPath);
                
                if (configFile.is_open()) {
                    try {
                        json j;
                        configFile >> j;
                        
                        info.presetName = j["preset_name"];
                        
                        for (auto& element : j["mapping"].items()) {
                            if (element.value().is_string()) {
                                info.mapping[element.key()] = element.value().get<std::string>();
                            }
                        }
                    } catch (const std::exception& e) {
                        info.isCorrupt = true;
                        info.presetName = dirName; // Use folder name if corrupt
                    }
                } else {
                    info.isCorrupt = true;
                    info.presetName = dirName;
                }
                presets.push_back(info);
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    // 2. Sort according to layerOrder
    std::vector<PresetInfo> sortedPresets;
    for (const auto& orderedName : layerOrder) {
        for (auto it = presets.begin(); it != presets.end(); ++it) {
            if (it->folderName == orderedName) {
                sortedPresets.push_back(*it);
                presets.erase(it);
                break;
            }
        }
    }
    // Append remaining
    for (const auto& remaining : presets) {
        sortedPresets.push_back(remaining);
    }
    
    presets = sortedPresets;
    SaveLayerConf(rootPath);
    
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

void PresetManager::MoveLayerUp(int index) {
    if (index > 0 && index < presets.size()) {
        std::swap(presets[index], presets[index - 1]);
        if (currentPresetIndex == index) currentPresetIndex = index - 1;
        else if (currentPresetIndex == index - 1) currentPresetIndex = index;
    }
}

void PresetManager::MoveLayerDown(int index) {
    if (index >= 0 && index < presets.size() - 1) {
        std::swap(presets[index], presets[index + 1]);
        if (currentPresetIndex == index) currentPresetIndex = index + 1;
        else if (currentPresetIndex == index + 1) currentPresetIndex = index;
    }
}

void PresetManager::SaveLayerConf(const std::string& rootPath) {
    std::string layerConfPath = rootPath + "/layer.conf";
    std::ofstream outFile(layerConfPath);
    if (outFile.is_open()) {
        for (const auto& preset : presets) {
            outFile << preset.folderName << "\n";
        }
    }
}
