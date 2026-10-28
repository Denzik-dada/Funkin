#include "hotkey.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

bool modMenuHotkeyPressed() {
    static bool prev = false;

    bool now = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;

    // реагируем только если в фокусе окно нашей игры
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId()) now = false;

    bool pressed = now && !prev;
    prev = now;
    return pressed;
}
#else
bool modMenuHotkeyPressed() { return false; }
#endif
