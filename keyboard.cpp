#include <iostream>
#include "keyboard.h"


KeyboardHook::KeyboardHook() {
    if (hook_occupied_.exchange(true)) {
        std::cerr << "[WARNING] attempted to spawn second KeyboardHook! creating empty instance\n";
        status = 2;
    }
    else {
        hook_handle_ = SetWindowsHookEx(WH_KEYBOARD_LL, ProcessKeys, GetModuleHandle(NULL), 0);
        if (hook_handle_ == NULL) {
            hook_occupied_.store(false);
            DWORD response = GetLastError();
            std::cerr << "[ERROR] KeyboardHook failed to create hook handle with error: ";
            std::cerr << response << "\n";
            status = 1;
        }
        else {
            std::cout << "KeyboardHook ready\n";
            status = 0;
        }
    }
}

KeyboardHook::~KeyboardHook() {
    if (hook_handle_ != NULL) { //this means you are the primary instance occupying the hook
        bool success = UnhookWindowsHookEx(hook_handle_);
        if (!success) {
            DWORD response = GetLastError();
            std::cerr << "[ERROR] KeyboardHook failed to unhook with error: " << response << "\n";
        }
        hook_occupied_.store(false);
    }
}

LRESULT CALLBACK KeyboardHook::ProcessKeys(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        const KBDLLHOOKSTRUCT& key_message = *reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool injected = key_message.flags & LLKHF_INJECTED;
        bool ping_signed = key_message.dwExtraInfo == ping_key_signature_;
        std::cout << key_message.time << ": " << key_message.vkCode;
        std::cout << ((injected) ? ", injected" : ", organic");
        switch (wParam) {
            case WM_KEYDOWN:
                std::cout << ", normal, down\n";
                break;
            case WM_KEYUP:
                std::cout << ", normal, up\n";
                break;
            case WM_SYSKEYDOWN:
                std::cout << ", system, down\n";
                break;
            case WM_SYSKEYUP:
                std::cout << ", system, up\n";
                break;
            default:
                std::cerr << "\n[WARNING] KeyboardHook unknown wParam: " << wParam << '\n';
                break;
        }
        switch (key_message.vkCode) {
            case VK_END:
                PostQuitMessage(0);
                break;
            case VK_VOLUME_DOWN:
                if (injected && (wParam == WM_KEYUP) && ping_signed) {
                    PingReply();
                }
                return 1;
            case VK_VOLUME_MUTE:
                if (wParam == WM_KEYUP) {
                    PingSend();
                }
                return 1;
        }
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

int KeyboardHook::Run() {
    if (hook_handle_ == NULL) return -1; //this means you are NOT the primary instance of the hook
    std::cout << "KeyboardHook starting to listen for keys\n";
    MSG message = {};
    while (true) {
        BOOL response = GetMessage(&message, NULL, 0, 0);
        if (response == 0) {
            std::cout << "KeyboardHook stopped listening for keys\n";
            return 0;
        }
        else if (response == -1) {
            DWORD error = GetLastError();
            std::cerr << "[WARNING] KeyboardHook failed to call GetMessage with error: ";
            std::cerr << error << '\n';
            return 1;
        }
        else if (message.message == WM_APP + ping_message_number_) {
            std::cout << "KeyboardHook received injected ping key\n";
        }
    }
}

bool KeyboardHook::PingSend() {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_VOLUME_DOWN;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    input.ki.dwExtraInfo = ping_key_signature_;
    UINT response = SendInput(1, &input, sizeof(INPUT));
    if (response == 1) {
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::cerr << "[WARNING] KeyboardHook failed to send Ping with error: " << error << "\n";
        return false;
    }
}

bool KeyboardHook::PingReply() {
    bool response = PostThreadMessage(GetCurrentThreadId(), WM_APP + ping_message_number_, 0, 0);
    if (response) {
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::cerr << "[WARNING] KeyboardHook failed to reply to Ping with error: " << error << "\n";
        return false;        
    }
}
