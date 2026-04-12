#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include "AudioEngine.h"
#include "PresetManager.h"

AudioEngine g_AudioEngine;
PresetManager g_PresetManager;
std::unordered_map<std::string, DWORD> g_PadFlashTime; // tracks when a pad was hit
const DWORD FLASH_DURATION = 150; // ms

std::string GetLogicalKeyFromVK(WPARAM wParam) {
    switch (wParam) {
        // Row 1
        case '1': case VK_DIVIDE: return "1";
        case '2': case VK_MULTIPLY: return "2";
        case '3': case VK_SUBTRACT: return "3";
        // Row 2
        case 'Q': case VK_NUMPAD7: return "Q";
        case 'W': case VK_NUMPAD8: return "W";
        case 'E': case VK_NUMPAD9: return "E";
        // Row 3
        case 'A': case VK_NUMPAD4: return "A";
        case 'S': case VK_NUMPAD5: return "S";
        case 'D': case VK_NUMPAD6: return "D";
        // Row 4
        case 'Z': case VK_NUMPAD1: return "Z";
        case 'X': case VK_NUMPAD2: return "X";
        case 'C': case VK_NUMPAD3: return "C";
        // Row 5
        case 'B': case VK_NUMPAD0: return "B";
        case 'N': case VK_DECIMAL: return "N";
        case VK_RETURN: return "M"; 
    }
    // M key specifically because VK_RETURN is also here
    if (wParam == 'M') return "M";
    
    return "";
}

const std::vector<std::vector<std::string>> PadLayout = {
    {"1", "2", "3"},
    {"Q", "W", "E"},
    {"A", "S", "D"},
    {"Z", "X", "C"},
    {"B", "N", "M"}
};

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
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 30, NULL); // Timer for UI repaint (flash effect fading)
            return 0;
            
        case WM_TIMER:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1; // Handled by WM_PAINT's double buffering

        case WM_LBUTTONDOWN: {
            int xPos = GET_X_LPARAM(lParam); 
            int yPos = GET_Y_LPARAM(lParam); 
            
            int padWidth = 80;
            int padHeight = 80;
            int startX = 20;
            int startY = 80;
            int padding = 10;
            
            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 3; ++c) {
                    int x = startX + c * (padWidth + padding);
                    int y = startY + r * (padHeight + padding);
                    
                    if (xPos >= x && xPos <= x + padWidth && 
                        yPos >= y && yPos <= y + padHeight) {
                        
                        std::string logicKey = PadLayout[r][c];
                        PromptWavSelection(hwnd, logicKey);
                        break;
                    }
                }
            }
            return 0;
        }

        case WM_KEYDOWN: {
            bool wasDown = ((lParam & (1 << 30)) != 0); // avoid spamming if held
            if (wasDown) return 0;

            if (wParam == VK_UP) {
                int count = g_PresetManager.GetPresetCount();
                if (count > 0) {
                    int next = (g_PresetManager.GetCurrentPresetIndex() - 1 + count) % count;
                    g_PresetManager.LoadPreset(next, g_AudioEngine);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            else if (wParam == VK_DOWN) {
                int count = g_PresetManager.GetPresetCount();
                if (count > 0) {
                    int next = (g_PresetManager.GetCurrentPresetIndex() + 1) % count;
                    g_PresetManager.LoadPreset(next, g_AudioEngine);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            else {
                std::string logicKey = GetLogicalKeyFromVK(wParam);
                if (!logicKey.empty()) {
                    std::string soundId = g_PresetManager.GetSoundIdForLogicalKey(logicKey);
                    if (!soundId.empty()) {
                        g_AudioEngine.PlaySoundById(soundId);
                        g_PadFlashTime[logicKey] = GetTickCount(); // For GUI flash
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                }
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
            std::string title = "DSX Drumb - " + g_PresetManager.GetCurrentPresetName();
            TextOutA(memDC, 20, 20, title.c_str(), title.length());
            
            std::string subtitle = "Up/Down arrows to change preset. Click pad to change sound.";
            TextOutA(memDC, 20, 45, subtitle.c_str(), subtitle.length());

            // Draw Pads
            int padWidth = 80;
            int padHeight = 80;
            int startX = 20;
            int startY = 80;
            int padding = 10;
            DWORD now = GetTickCount();

            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 3; ++c) {
                    std::string key = PadLayout[r][c];
                    
                    int x = startX + c * (padWidth + padding);
                    int y = startY + r * (padHeight + padding);
                    
                    RECT padRect = { x, y, x + padWidth, y + padHeight };
                    
                    int intensity = 0;
                    if (g_PadFlashTime.find(key) != g_PadFlashTime.end()) {
                        DWORD elapsed = now - g_PadFlashTime[key];
                        if (elapsed < FLASH_DURATION) {
                            intensity = 255 - (255 * elapsed / FLASH_DURATION);
                        }
                    }
                    
                    HBRUSH pBrush = CreateSolidBrush(RGB(50 + intensity/2, 50 + intensity, 50 + intensity/2));
                    FillRect(memDC, &padRect, pBrush);
                    DeleteObject(pBrush);
                    
                    // Draw Key Name
                    SetTextColor(memDC, RGB(200, 200, 200));
                    DrawTextA(memDC, key.c_str(), -1, &padRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }

            // Blit to screen
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
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

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "DSX Drumb",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, // disable resize
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 600,
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
