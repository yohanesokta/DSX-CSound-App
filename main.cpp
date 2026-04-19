#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include "AudioEngine.h"
#include "PresetManager.h"
#include <thread>
#include <atomic>
#include <chrono>

struct RecordEvent {
    std::string logicKey;
    DWORD delayBeforeNextMs;
};

std::vector<RecordEvent> g_RecordedEvents;
bool g_IsRecordingArmed = false;
bool g_IsRecording = false;
std::atomic<bool> g_IsPlaying = false;
DWORD g_LastEventTime = 0;

HFONT g_hFontTitle = NULL;
HFONT g_hFontNormal = NULL;
HFONT g_hFontSmall = NULL;

AudioEngine g_AudioEngine;
PresetManager g_PresetManager;
std::unordered_map<std::string, DWORD> g_PadFlashTime; // tracks when a pad was hit
const DWORD FLASH_DURATION = 150; // ms

std::unordered_map<WPARAM, std::string> g_KeyToPad;
std::unordered_map<std::string, WPARAM> g_PadToKey;
std::string g_WaitingForKeyForPad = "";

struct PadDef {
    std::string logicKey;
    int x, y, w, h;
};
std::vector<PadDef> g_Pads;

std::string VKToStr(WPARAM vk) {
    if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
    if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
    switch(vk) {
        case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER";
        case VK_NUMPAD0: return "NUM0"; case VK_NUMPAD1: return "NUM1";
        case VK_NUMPAD2: return "NUM2"; case VK_NUMPAD3: return "NUM3";
        case VK_NUMPAD4: return "NUM4"; case VK_NUMPAD5: return "NUM5";
        case VK_NUMPAD6: return "NUM6"; case VK_NUMPAD7: return "NUM7";
        case VK_NUMPAD8: return "NUM8"; case VK_NUMPAD9: return "NUM9";
        case VK_DECIMAL: return "NUM."; case VK_ADD: return "NUM+";
        case VK_SUBTRACT: return "NUM-"; case VK_MULTIPLY: return "NUM*";
        case VK_DIVIDE: return "NUM/";
        case VK_UP: return "UP"; case VK_DOWN: return "DOWN";
        case VK_OEM_1: return ";"; case VK_OEM_PLUS: return "="; 
        case VK_OEM_COMMA: return ","; case VK_OEM_MINUS: return "-"; 
        case VK_OEM_PERIOD: return "."; case VK_OEM_2: return "/"; 
        case VK_OEM_3: return "`"; case VK_OEM_4: return "["; 
        case VK_OEM_5: return "\\"; case VK_OEM_6: return "]"; 
        case VK_OEM_7: return "'"; 
    }
    return std::to_string(vk);
}

WPARAM StrToVK(const std::string& str) {
    if (str.length() == 1) {
        char c = toupper(str[0]);
        if (c >= '0' && c <= '9') return c;
        if (c >= 'A' && c <= 'Z') return c;
        switch(c) {
            case ';': return VK_OEM_1; case '=': return VK_OEM_PLUS;
            case ',': return VK_OEM_COMMA; case '-': return VK_OEM_MINUS;
            case '.': return VK_OEM_PERIOD; case '/': return VK_OEM_2;
            case '`': return VK_OEM_3; case '[': return VK_OEM_4;
            case '\\': return VK_OEM_5; case ']': return VK_OEM_6;
            case '\'': return VK_OEM_7;
        }
    }
    if (str == "SPACE") return VK_SPACE;
    if (str == "ENTER") return VK_RETURN;
    if (str == "NUM0") return VK_NUMPAD0; if (str == "NUM1") return VK_NUMPAD1;
    if (str == "NUM2") return VK_NUMPAD2; if (str == "NUM3") return VK_NUMPAD3;
    if (str == "NUM4") return VK_NUMPAD4; if (str == "NUM5") return VK_NUMPAD5;
    if (str == "NUM6") return VK_NUMPAD6; if (str == "NUM7") return VK_NUMPAD7;
    if (str == "NUM8") return VK_NUMPAD8; if (str == "NUM9") return VK_NUMPAD9;
    if (str == "NUM.") return VK_DECIMAL; if (str == "NUM+") return VK_ADD;
    if (str == "NUM-") return VK_SUBTRACT; if (str == "NUM*") return VK_MULTIPLY;
    if (str == "NUM/") return VK_DIVIDE;
    if (str == "UP") return VK_UP; if (str == "DOWN") return VK_DOWN;
    if (str == "LEFT") return VK_LEFT; if (str == "RIGHT") return VK_RIGHT;

    try { return (WPARAM)std::stoi(str); } catch(...) {}
    return 0;
}

void LoadLayoutConf() {
    g_KeyToPad.clear();
    g_PadToKey.clear();
    char buf[256];
    char fullPath[MAX_PATH];
    GetFullPathNameA("./layout.conf", MAX_PATH, fullPath, NULL);
    for (int i=1; i<=14; ++i) {
        std::string keyName = std::to_string(i);
        std::string padName = "Pad" + keyName;
        GetPrivateProfileStringA("PAD", keyName.c_str(), "", buf, 256, fullPath);
        std::string valStr = buf;
        WPARAM vk = StrToVK(valStr);
        if (vk != 0) {
            g_PadToKey[padName] = vk;
            g_KeyToPad[vk] = padName;
        }
    }
}

void SaveLayoutConf() {
    char fullPath[MAX_PATH];
    GetFullPathNameA("./layout.conf", MAX_PATH, fullPath, NULL);
    for (int i=1; i<=14; ++i) {
        std::string keyName = std::to_string(i);
        std::string padName = "Pad" + keyName;
        if (g_PadToKey.find(padName) != g_PadToKey.end()) {
            std::string valStr = VKToStr(g_PadToKey[padName]);
            WritePrivateProfileStringA("PAD", keyName.c_str(), valStr.c_str(), fullPath);
        }
    }
}

void InitPads() {
    int padWidth = 100;
    int padHeight = 80;
    int startX = 20;
    int startY = 70;
    int padding = 10;
    int padIdx = 1;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            std::string logicKey = "Pad" + std::to_string(padIdx++);
            g_Pads.push_back({logicKey, startX + c * (padWidth + padding), startY + r * (padHeight + padding), padWidth, padHeight});
        }
    }
    g_Pads.push_back({"Pad13", 390, 440, padWidth, padHeight});
    g_Pads.push_back({"Pad14", 500, 440, padWidth, padHeight});
}

void PromptWavSelection(HWND hwnd, const std::string& logicKey) {
    char filename[MAX_PATH];
    filename[0] = '\0';

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "wav";

    std::string presetPath = g_PresetManager.GetCurrentPresetBasePath();
    char fullPath[MAX_PATH];
    if (GetFullPathNameA(presetPath.c_str(), MAX_PATH, fullPath, NULL)) {
        ofn.lpstrInitialDir = fullPath;
    }

    if (GetOpenFileNameA(&ofn)) {
        std::string selectedPath = ofn.lpstrFile;
        size_t pos = selectedPath.find_last_of("\\/");
        if (pos != std::string::npos) {
            std::string justFile = selectedPath.substr(pos + 1);
            g_PresetManager.UpdateMapping(logicKey, justFile, g_AudioEngine);
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
}

#define IDM_HELP 1001
#define IDM_FILE_SAVE_REC 1002
#define IDM_FILE_LOAD_REC 1003

void SaveRecord(HWND hwnd) {
    if (g_RecordedEvents.empty()) {
        MessageBoxA(hwnd, "Belum ada record yang berjalan untuk disimpan!", "Info", MB_OK | MB_ICONWARNING);
        return;
    }
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "DSX Record\0*.dsxrec\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "dsxrec";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    if (GetSaveFileNameA(&ofn)) {
        FILE* f = NULL;
        fopen_s(&f, filename, "w");
        if (f) {
            for (const auto& ev : g_RecordedEvents) {
                fprintf(f, "%s %u\n", ev.logicKey.c_str(), (unsigned)ev.delayBeforeNextMs);
            }
            fclose(f);
            MessageBoxA(hwnd, "Recording berhasil disimpan.", "Success", MB_OK);
        }
    }
}

void LoadRecord(HWND hwnd) {
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "DSX Record\0*.dsxrec\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameA(&ofn)) {
        FILE* f = NULL;
        fopen_s(&f, filename, "r");
        if (f) {
            std::vector<RecordEvent> tempEvents;
            char key[256];
            unsigned int del;
            while (fscanf_s(f, "%255s %u", key, (unsigned int)sizeof(key), &del) == 2) {
                tempEvents.push_back({key, del});
            }
            fclose(f);
            
            g_IsRecording = false;
            g_IsRecordingArmed = false;
            g_IsPlaying = false;
            g_RecordedEvents = std::move(tempEvents);
            g_LastEventTime = 0;
            
            InvalidateRect(hwnd, NULL, FALSE);
            MessageBoxA(hwnd, "Recording berhasil diload!\nSilakan tekan ENTER untuk memutar.", "Success", MB_OK | MB_ICONINFORMATION);
        }
    }
}

LRESULT CALLBACK HelpWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 28)); // Dark modern flat
            FillRect(memDC, &rect, bgBrush);
            DeleteObject(bgBrush);
            
            SetBkMode(memDC, TRANSPARENT);
            
            // Draw title
            SetTextColor(memDC, RGB(255, 200, 100)); // Accent color
            HFONT titleFont = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(memDC, titleFont);
            
            RECT titleRect = { 20, 20, rect.right - 20, 60 };
            DrawTextA(memDC, "DSX Drumb - Panduan Penggunaan", -1, &titleRect, DT_LEFT | DT_TOP);
            
            SelectObject(memDC, oldFont);
            DeleteObject(titleFont);
            
            // Draw body text
            SetTextColor(memDC, RGB(220, 220, 220));
            HFONT bodyFont = CreateFontA(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            oldFont = (HFONT)SelectObject(memDC, bodyFont);
            
            const char* helpText = 
                "+ KLIK KIRI PAD: Mengganti file suara WAV\n\n"
                "+ KLIK KANAN PAD: Bind / Atur tombol keyboard untuk pad tsb.\n\n"
                "+ PANAH ATAS/BAWAH: Pindah preset layer\n\n"
                "+ PANAH KIRI/KANAN (+/-): Mengatur Master Volume\n\n"
                "+ TOMBOL TAB: Standby / Stop Record Sequencer\n"
                "   -> Tekan TAB, lalu mainkan pad untuk mulai merekam.\n"
                "   -> Jeda antar pukulan akan terekam sangat presisi.\n\n"
                "+ TOMBOL ENTER: Play / Stop Looping Sequencer\n\n"
                "+ KLIK [ ^ atau v ] di Layer List: Pindah urutan slot preset\n\n"
                "+ KLIK TOMBOL SAVE: Menyimpan preset";

            RECT bodyRect = { 20, 70, rect.right - 20, rect.bottom - 20 };
            DrawTextA(memDC, helpText, -1, &bodyRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
            
            SelectObject(memDC, oldFont);
            DeleteObject(bodyFont);
            
            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_hFontNormal = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_hFontSmall = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

            HMENU hMenu = CreateMenu();
            
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVE_REC, "Save Record...");
            AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_LOAD_REC, "Load Record...");
            
            AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");
            AppendMenuA(hMenu, MF_STRING, IDM_HELP, "Help");
            SetMenu(hwnd, hMenu);

            InitPads();
            LoadLayoutConf();
            SetTimer(hwnd, 1, 30, NULL); // Timer for UI repaint (flash effect fading)
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == IDM_HELP) {
                HWND hHelp = CreateWindowEx(
                    WS_EX_TOOLWINDOW, "DSX_Help_Class", "Bantuan / Help",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                    CW_USEDEFAULT, CW_USEDEFAULT, 500, 520,
                    hwnd, NULL, GetModuleHandle(NULL), NULL
                );
                ShowWindow(hHelp, SW_SHOW);
            } else if (LOWORD(wParam) == IDM_FILE_SAVE_REC) {
                SaveRecord(hwnd);
            } else if (LOWORD(wParam) == IDM_FILE_LOAD_REC) {
                LoadRecord(hwnd);
            }
            return 0;
        }

        case WM_TIMER:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1; // Handled by WM_PAINT's double buffering

        case WM_LBUTTONDOWN: {
            int xPos = GET_X_LPARAM(lParam); 
            int yPos = GET_Y_LPARAM(lParam); 
            
            // Check if Save button was clicked
            if (g_PresetManager.IsCurrentPresetEdited()) {
                if (xPos >= 300 && xPos <= 370 && yPos >= 15 && yPos <= 40) {
                    g_PresetManager.SaveCurrentPreset();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Check Layer List clicks
            int layerStartX = 390;
            int layerWidth = 220;
            int layerItemHeight = 40;
            int layerStartY = 60;
            const auto& presets = g_PresetManager.GetPresets();

            for (int i = 0; i < presets.size(); ++i) {
                int y = layerStartY + i * layerItemHeight;
                if (xPos >= layerStartX && xPos <= layerStartX + layerWidth && yPos >= y && yPos <= y + layerItemHeight) {
                    // Check UP arrow [^] (e.g. width=20, offset=170)
                    if (xPos >= layerStartX + 170 && xPos <= layerStartX + 190) {
                        g_PresetManager.MoveLayerUp(i);
                        g_PresetManager.SaveLayerConf("./soundpack");
                    }
                    // Check DOWN arrow [v] (e.g. width=20, offset=195)
                    else if (xPos >= layerStartX + 195 && xPos <= layerStartX + 215) {
                        g_PresetManager.MoveLayerDown(i);
                        g_PresetManager.SaveLayerConf("./soundpack");
                    }
                    // Otherwise load the preset
                    else if (!presets[i].isCorrupt) {
                        g_PresetManager.LoadPreset(i, g_AudioEngine);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            for (const auto& pad : g_Pads) {
                if (xPos >= pad.x && xPos <= pad.x + pad.w && 
                    yPos >= pad.y && yPos <= pad.y + pad.h) {
                    PromptWavSelection(hwnd, pad.logicKey);
                    break;
                }
            }
            return 0;
        }

        case WM_RBUTTONDOWN: {
            int xPos = GET_X_LPARAM(lParam); 
            int yPos = GET_Y_LPARAM(lParam); 
            
            // Fix: jika klik kanan ke 2 adalah cancel
            if (!g_WaitingForKeyForPad.empty()) {
                g_WaitingForKeyForPad = "";
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            for (const auto& pad : g_Pads) {
                if (xPos >= pad.x && xPos <= pad.x + pad.w && 
                    yPos >= pad.y && yPos <= pad.y + pad.h) {
                    g_WaitingForKeyForPad = pad.logicKey;
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
            }
            return 0;
        }

        case WM_KEYDOWN: {
            bool wasDown = ((lParam & (1 << 30)) != 0); // avoid spamming if held
            if (wasDown) return 0;

            if (!g_WaitingForKeyForPad.empty()) {
                if (wParam == VK_ESCAPE) {
                    g_WaitingForKeyForPad = "";
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                
                WPARAM oldKey = g_PadToKey[g_WaitingForKeyForPad];
                if (oldKey != 0) {
                    g_KeyToPad.erase(oldKey);
                }
                
                std::string oldPad = g_KeyToPad[wParam];
                if (!oldPad.empty()) {
                    g_PadToKey.erase(oldPad);
                }
                
                g_PadToKey[g_WaitingForKeyForPad] = wParam;
                g_KeyToPad[wParam] = g_WaitingForKeyForPad;
                SaveLayoutConf();
                
                g_WaitingForKeyForPad = "";
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            std::string logicKey = "";
            if (g_KeyToPad.find(wParam) != g_KeyToPad.end()) {
                logicKey = g_KeyToPad[wParam];
            }
            
            if (!logicKey.empty()) {
                std::string soundId = g_PresetManager.GetSoundIdForLogicalKey(logicKey);
                if (!soundId.empty()) {
                    g_AudioEngine.PlaySoundById(soundId);
                    g_PadFlashTime[logicKey] = GetTickCount(); // For GUI flash
                    InvalidateRect(hwnd, NULL, FALSE);

                    if (g_IsRecordingArmed) {
                        g_IsRecordingArmed = false;
                        g_IsRecording = true;
                        g_RecordedEvents.push_back({logicKey, 0});
                        g_LastEventTime = GetTickCount();
                    } else if (g_IsRecording) {
                        DWORD now = GetTickCount();
                        if (g_RecordedEvents.empty()) {
                            g_RecordedEvents.push_back({logicKey, 0});
                            g_LastEventTime = now;
                        } else {
                            g_RecordedEvents.back().delayBeforeNextMs = now - g_LastEventTime;
                            g_RecordedEvents.push_back({logicKey, 0});
                            g_LastEventTime = now;
                        }
                    }
                }
            } else if (wParam == VK_UP) {
                int count = g_PresetManager.GetPresetCount();
                if (count > 0) {
                    int next = (g_PresetManager.GetCurrentPresetIndex() - 1 + count) % count;
                    g_PresetManager.LoadPreset(next, g_AudioEngine);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_DOWN) {
                int count = g_PresetManager.GetPresetCount();
                if (count > 0) {
                    int next = (g_PresetManager.GetCurrentPresetIndex() + 1) % count;
                    g_PresetManager.LoadPreset(next, g_AudioEngine);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (wParam == VK_LEFT || wParam == VK_SUBTRACT || wParam == VK_OEM_MINUS) {
                float v = g_AudioEngine.GetVolume() - 0.05f;
                g_AudioEngine.SetVolume(v);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == VK_RIGHT || wParam == VK_ADD || wParam == VK_OEM_PLUS) {
                float v = g_AudioEngine.GetVolume() + 0.05f;
                g_AudioEngine.SetVolume(v);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == VK_TAB) {
                if (!g_IsRecording && !g_IsRecordingArmed) {
                    g_IsRecordingArmed = true;
                    g_IsRecording = false;
                    g_RecordedEvents.clear();
                    g_LastEventTime = 0;
                } else if (g_IsRecording || g_IsRecordingArmed) {
                    g_IsRecordingArmed = false;
                    g_IsRecording = false;
                    if (!g_RecordedEvents.empty()) {
                        // Simpan delay antara pad terakhir ke tombol tab biar tidak langsung loop
                        DWORD stopDelay = GetTickCount() - g_LastEventTime;
                        g_RecordedEvents.back().delayBeforeNextMs = stopDelay;
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (wParam == VK_RETURN) { // ENTER
                if (!g_IsPlaying && !g_RecordedEvents.empty()) {
                    g_IsPlaying = true;
                    std::thread([](HWND hwndTarget) {
                        while (g_IsPlaying) {
                            for (size_t i = 0; i < g_RecordedEvents.size(); ++i) {
                                if (!g_IsPlaying) break;
                                
                                auto& ev = g_RecordedEvents[i];
                                std::string soundId = g_PresetManager.GetSoundIdForLogicalKey(ev.logicKey);
                                if (!soundId.empty()) {
                                    g_AudioEngine.PlaySoundById(soundId);
                                    g_PadFlashTime[ev.logicKey] = GetTickCount(); 
                                }
                                
                                if (ev.delayBeforeNextMs > 0) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(ev.delayBeforeNextMs));
                                }
                            }
                        }
                    }, hwnd).detach();
                } else if (g_IsPlaying) {
                    g_IsPlaying = false;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            // Draw background
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(memDC, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            // Draw Title
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            HFONT oldFont = (HFONT)SelectObject(memDC, g_hFontTitle);
            
            std::string title = "DSX Drumb - " + g_PresetManager.GetCurrentPresetName();
            if (g_PresetManager.IsCurrentPresetEdited()) {
                title += " [EDITED]";
            }
            TextOutA(memDC, 20, 10, title.c_str(), title.length());
            
            SelectObject(memDC, g_hFontNormal); // Use normal smooth font for body
            
            if (g_IsRecordingArmed) {
                SetTextColor(memDC, RGB(255, 150, 50)); // Orange
                TextOutA(memDC, 450, 10, "* STANDBY...", 12);
            } else if (g_IsRecording) {
                SetTextColor(memDC, RGB(255, 50, 50)); // Red
                TextOutA(memDC, 450, 10, "* RECORDING...", 14);
            } else if (g_IsPlaying) {
                SetTextColor(memDC, RGB(50, 255, 50)); // Green
                TextOutA(memDC, 450, 10, "> PLAYING LOOP", 14);
            }
            
            std::string subtitle = "Up/Down=Preset, Left/Right=+/- Vol, Click=Set Sound";
            TextOutA(memDC, 20, 35, subtitle.c_str(), subtitle.length());

            char volText[64];
            snprintf(volText, sizeof(volText), "Volume: %d%%", (int)(g_AudioEngine.GetVolume() * 100.0f + 0.5f));
            SetTextColor(memDC, RGB(255, 200, 100)); // Yellowish volume text
            TextOutA(memDC, 20, 450, volText, strlen(volText));

            // Draw Save Button if edited
            if (g_PresetManager.IsCurrentPresetEdited()) {
                RECT btnRect = { 300, 15, 370, 40 };
                HBRUSH btnBrush = CreateSolidBrush(RGB(0, 120, 215)); // Blue save button
                FillRect(memDC, &btnRect, btnBrush);
                DeleteObject(btnBrush);
                
                SetTextColor(memDC, RGB(255, 255, 255));
                std::string btnText = "SAVE";
                DrawTextA(memDC, btnText.c_str(), -1, &btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Draw Layer Panel Header
            SetTextColor(memDC, RGB(220, 220, 220));
            std::string layerTitle = "--- Layers ---";
            TextOutA(memDC, 400, 20, layerTitle.c_str(), layerTitle.length());

            int layerStartX = 390;
            int layerWidth = 220;
            int layerItemHeight = 40;
            int layerStartY = 60;
            const auto& presets = g_PresetManager.GetPresets();

            for (int i = 0; i < presets.size(); ++i) {
                int y = layerStartY + i * layerItemHeight;
                RECT itemRect = { layerStartX, y, layerStartX + layerWidth, y + layerItemHeight - 4 };
                
                // Highlight active layer
                bool isActive = (i == g_PresetManager.GetCurrentPresetIndex());
                HBRUSH itemBrush = CreateSolidBrush(isActive ? RGB(80, 80, 100) : RGB(40, 40, 40));
                FillRect(memDC, &itemRect, itemBrush);
                DeleteObject(itemBrush);
                
                // Name
                std::string nameText = presets[i].folderName;
                if (!presets[i].presetName.empty() && presets[i].presetName != presets[i].folderName) {
                    nameText += " (" + presets[i].presetName + ")";
                }

                RECT textRect = itemRect;
                textRect.left += 5;
                if (presets[i].isCorrupt) {
                    SetTextColor(memDC, RGB(255, 100, 100)); // Red text if corrupt
                    nameText += " [corrupt]";
                } else {
                    SetTextColor(memDC, isActive ? RGB(255, 255, 255) : RGB(180, 180, 180));
                }
                DrawTextA(memDC, nameText.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // Up Button
                RECT upBtn = { layerStartX + 170, y, layerStartX + 190, y + layerItemHeight - 4 };
                HBRUSH btnBrush = CreateSolidBrush(RGB(60, 60, 60));
                FillRect(memDC, &upBtn, btnBrush);
                SetTextColor(memDC, RGB(200, 200, 200));
                DrawTextA(memDC, "^", -1, &upBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                // Down Button
                RECT downBtn = { layerStartX + 195, y, layerStartX + 215, y + layerItemHeight - 4 };
                FillRect(memDC, &downBtn, btnBrush);
                
                SelectObject(memDC, g_hFontSmall); 
                DrawTextA(memDC, "v", -1, &downBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(memDC, g_hFontNormal);
                
                DeleteObject(btnBrush);
            }

            // Draw Pads
            DWORD now = GetTickCount();

            for (const auto& pad : g_Pads) {
                std::string key = pad.logicKey;
                std::string fname = g_PresetManager.GetFileNameForLogicalKey(key);
                bool isDisabled = fname.empty();
                
                bool isWaiting = (g_WaitingForKeyForPad == key);
                
                int intensity = 0;
                if (!isWaiting && g_PadFlashTime.find(key) != g_PadFlashTime.end()) {
                    DWORD elapsed = now - g_PadFlashTime[key];
                    if (elapsed < FLASH_DURATION) {
                        intensity = 255 - (255 * elapsed / FLASH_DURATION);
                    }
                }
                
                // Shadow
                HBRUSH shadowBrush = CreateSolidBrush(RGB(15, 15, 15));
                HPEN shadowPen = CreatePen(PS_NULL, 0, 0);
                HPEN oldPen = (HPEN)SelectObject(memDC, shadowPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, shadowBrush);
                
                RoundRect(memDC, pad.x + 6, pad.y + 6, pad.x + pad.w + 6, pad.y + pad.h + 6, 8, 8);
                
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBrush);
                DeleteObject(shadowPen);
                DeleteObject(shadowBrush);

                HBRUSH pBrush;
                if (isWaiting) {
                    pBrush = CreateSolidBrush(RGB(200, 150, 0)); // Yellowish
                } else if (isDisabled) {
                    pBrush = CreateSolidBrush(RGB(30, 30, 30)); // Darker disabled
                } else {
                    pBrush = CreateSolidBrush(RGB(50 + intensity, 50, 50));
                }

                RECT drawnRect = { pad.x, pad.y, pad.x + pad.w, pad.y + pad.h };
                
                HPEN padPen = CreatePen(PS_SOLID, 1, RGB(70, 70, 70)); // Subtle border
                oldPen = (HPEN)SelectObject(memDC, padPen);
                oldBrush = (HBRUSH)SelectObject(memDC, pBrush);
                
                RoundRect(memDC, drawnRect.left, drawnRect.top, drawnRect.right, drawnRect.bottom, 8, 8);
                
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBrush);
                DeleteObject(padPen);
                DeleteObject(pBrush);
                
                // Draw Key Name and Filename
                SetTextColor(memDC, isDisabled ? RGB(100, 100, 100) : (isWaiting ? RGB(255, 255, 255) : RGB(220, 220, 220)));
                
                RECT upperRect = drawnRect;
                upperRect.bottom = drawnRect.top + pad.h / 2 + 10;
                
                if (isWaiting) {
                    DrawTextA(memDC, "PRESS KEY", -1, &upperRect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
                } else {
                    DrawTextA(memDC, key.c_str(), -1, &upperRect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
                }
                
                if (!isWaiting) {
                    std::string mappedKeyStr = "";
                    if (g_PadToKey.find(key) != g_PadToKey.end()) {
                        mappedKeyStr = "" + VKToStr(g_PadToKey[key]) + "";
                    } else {
                        mappedKeyStr = "?";
                    }
                    
                    RECT topCenterRect = drawnRect;
                    topCenterRect.top += 5; // offset a bit from top
                    SetTextColor(memDC, RGB(180, 180, 180));
                    DrawTextA(memDC, mappedKeyStr.c_str(), -1, &topCenterRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
                }
                
                RECT lowerRect = drawnRect;
                lowerRect.top = drawnRect.top + pad.h / 2 + 10;
                
                std::string truncFname = isDisabled ? "NONE" : fname;
                if (truncFname.length() > 12) {
                    // Truncate long filenames
                    truncFname = truncFname.substr(0, 10) + "..";
                }
                
                SelectObject(memDC, g_hFontSmall);
                SetTextColor(memDC, isDisabled ? RGB(80, 80, 80) : RGB(150, 150, 150));
                DrawTextA(memDC, truncFname.c_str(), -1, &lowerRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
                SelectObject(memDC, g_hFontNormal);
            }

            // Draw Copyright
            SetTextColor(memDC, RGB(100, 100, 100));
            std::string copyright = "Copyright \u00A9 2026 Yohanes Oktanio. Built with passion.";
            RECT copyRect = { 20, 550, 630, 580 };
            DrawTextA(memDC, copyright.c_str(), -1, &copyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Blit to screen
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldFont);
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!g_AudioEngine.Initialize()) {
        MessageBoxA(NULL, "Failed to initialize XAudio2", "Error", MB_OK);
        return -1;
    }

    g_PresetManager.DiscoverPresets("./soundpack");
    if (g_PresetManager.GetPresetCount() > 0) {
        g_PresetManager.LoadPreset(0, g_AudioEngine);
    }

    const char CLASS_NAME[] = "DSX_Drumb_Class";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    WNDCLASS wcHelp = { };
    wcHelp.lpfnWndProc = HelpWindowProc;
    wcHelp.hInstance = hInstance;
    wcHelp.lpszClassName = "DSX_Help_Class";
    wcHelp.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcHelp.hbrBackground = CreateSolidBrush(RGB(25, 25, 28));
    RegisterClass(&wcHelp);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "DSX Drumb",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // disable resize
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 650,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
