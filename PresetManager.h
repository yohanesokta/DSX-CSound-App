#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "AudioEngine.h"

struct PresetInfo {
    std::string presetName;
    std::string basePath;
    std::unordered_map<std::string, std::string> mapping;
};

class PresetManager {
public:
    PresetManager();

    bool DiscoverPresets(const std::string& rootPath);
    bool LoadPreset(int index, AudioEngine& audioEngine);
    
    int GetCurrentPresetIndex() const { return currentPresetIndex; }
    int GetPresetCount() const { return (int)presets.size(); }
    const std::string& GetCurrentPresetName() const;
    
    // Convert pad character (like "1", "Q", "A") to sound ID if available
    std::string GetSoundIdForLogicalKey(const std::string& logicalKey) const;

private:
    std::vector<PresetInfo> presets;
    int currentPresetIndex = -1;
};
