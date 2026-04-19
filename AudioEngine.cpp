#include "AudioEngine.h"
#include <iostream>

#pragma comment(lib, "xaudio2.lib")

AudioEngine::AudioEngine() : pMasterVoice(nullptr) {}

AudioEngine::~AudioEngine() {
    UnloadAll();
    if (pXAudio2) {
        pXAudio2->StopEngine();
    }
    CoUninitialize();
}

bool AudioEngine::Initialize() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;

    hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return false;

    hr = pXAudio2->CreateMasteringVoice(&pMasterVoice);
    if (FAILED(hr)) return false;

    return true;
}

bool AudioEngine::LoadSound(const std::string& id, const std::string& filePath) {
    WavData data = WavLoader::LoadWavFile(filePath);
    if (!data.isValid) return false;

    SoundBuffer sb;
    sb.wavData = std::move(data);
    
    sb.buffer.AudioBytes = (UINT32)sb.wavData.audioData.size();
    sb.buffer.pAudioData = sb.wavData.audioData.data();
    sb.buffer.Flags = XAUDIO2_END_OF_STREAM;
    
    sounds[id] = std::move(sb);

    // Create a pool of voices for overlapping plays
    VoicePool pool;
    for (int i = 0; i < VOICES_PER_SOUND; ++i) {
        IXAudio2SourceVoice* pSourceVoice;
        HRESULT hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&sounds[id].wavData.wfx);
        if (SUCCEEDED(hr)) {
            pool.voices.push_back(pSourceVoice);
        }
    }
    voicePools[id] = pool;

    return true;
}

void AudioEngine::PlaySoundById(const std::string& id) {
    auto it = sounds.find(id);
    if (it == sounds.end()) return;

    auto& pool = voicePools[id];
    if (pool.voices.empty()) return;

    IXAudio2SourceVoice* pVoice = pool.voices[pool.nextVoice];
    
    // Stop and flush existing buffer if it's currently playing
    pVoice->Stop();
    pVoice->FlushSourceBuffers();

    // Submit and play
    pVoice->SubmitSourceBuffer(&it->second.buffer);
    pVoice->Start(0);

    // Round-robin
    pool.nextVoice = (pool.nextVoice + 1) % pool.voices.size();
}

void AudioEngine::UnloadAll() {
    for (auto& pair : voicePools) {
        for (auto pVoice : pair.second.voices) {
            if (pVoice) {
                pVoice->Stop();
                pVoice->DestroyVoice();
            }
        }
    }
    voicePools.clear();
    sounds.clear();
}

void AudioEngine::SetVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    m_volume = volume;
    if (pMasterVoice) {
        pMasterVoice->SetVolume(m_volume);
    }
}

float AudioEngine::GetVolume() const {
    return m_volume;
}
