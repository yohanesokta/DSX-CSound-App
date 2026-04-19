#pragma once

#include <xaudio2.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "WavLoader.h"
#include <wrl/client.h>

struct SoundBuffer {
    WavData wavData;
    XAUDIO2_BUFFER buffer = {0};
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize();
    bool LoadSound(const std::string& id, const std::string& filePath);
    void PlaySoundById(const std::string& id);
    void UnloadAll();

    void SetVolume(float volume);
    float GetVolume() const;

private:
    float m_volume = 1.0f;
    Microsoft::WRL::ComPtr<IXAudio2> pXAudio2;
    IXAudio2MasteringVoice* pMasterVoice;
    
    std::unordered_map<std::string, SoundBuffer> sounds;
    
    // We pool voices per buffer to allow polyphony (overlapping)
    struct VoicePool {
        std::vector<IXAudio2SourceVoice*> voices;
        int nextVoice = 0;
    };
    std::unordered_map<std::string, VoicePool> voicePools;
    
    const int VOICES_PER_SOUND = 4; // allow 4 overlapping instances of the same sound
};
