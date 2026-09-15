#include <iostream>
#include <format>
#include <thread>
#include "keyboard.h"

KeyboardHook::~KeyboardHook() {
    if (worker_.joinable()) {
        std::cerr << "[WARNING] close KeyboardHook before letting it go out of scope\n";
        Stop(true);
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

bool KeyboardHook::Start() {
    if (worker_status_.load() != WorkerStatus::PreStart) {
        std::cerr << "[ERROR] KeyboardHook tried to start a worker but already owns one!\n"; 
        return false;
    }
    worker_ = std::thread(&KeyboardHook::Run, this);
    worker_status_.wait(WorkerStatus::PreStart);
    if (worker_status_.load() != WorkerStatus::Running) {
        Stop();
        return false;
    }
    std::cout << "KeyboardHook successfully started worker thread\n";
    return true;
}

bool KeyboardHook::Stop(bool force) {
    if (!worker_.joinable() || (worker_status_.load() == WorkerStatus::PreStart)) {
        std::cerr << "[WARNING] KeyboardHook tried to kill worker thread that is uninitialized!\n";
        return false;
    }
    if (worker_status_.load() == WorkerStatus::Running) {
        bool response = PostThreadMessage(worker_thread_id_.load(), WM_QUIT, 0, 0);
        if (!response) {
            DWORD error = GetLastError();
            std::cerr << std::format("[ERROR] KeyboardHook failed to gently kill worker with error:"
                                     " {}\n", error);
            if (force) {
                std::cerr << "[WARNING] KeyboardHook will try to close the thread forcefully\n";
            }
            else {
                return false;
            }
        }
    }
    worker_.join();
    std::cout << "KeyboardHook successfully closed worker thread\n";
    worker_thread_id_.store(0);
    worker_status_.store(WorkerStatus::PreStart);
    return true;
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

void KeyboardHook::Run() {
    worker_thread_id_.store(GetCurrentThreadId());
    HHOOK hook_handle = Hook();
    if (hook_handle == NULL) {
        std::cerr << "[WARNING] KeyboardHook worker aborting\n";
        worker_status_.store(WorkerStatus::QuitHookError);
        worker_status_.notify_all(); //add to any future alternative paths or Start blocks forever
        return;
    }
    std::cout << "KeyboardHook worker starting to listen for keys\n";
    worker_status_.store(WorkerStatus::Running);
    worker_status_.notify_all(); //add to any future alternative paths or Start blocks forever
    MSG message = {};
    while (true) {
        BOOL response = GetMessage(&message, NULL, 0, 0);
        if (response == 0) {
            std::cout << "KeyboardHook worker stopped listening for keys\n";
            worker_status_.store(WorkerStatus::QuitRequest);
            break;
        }
        else if (response == -1) {
            DWORD error = GetLastError();
            std::cerr << std::format("[WARNING] KeyboardHook worker failed to call GetMessage with "
                                        "error: {}\n", error);
            worker_status_.store(WorkerStatus::QuitMessageError);
            break;
        }
        else if (message.message == WM_APP + ping_message_number_) {
            std::cout << "KeyboardHook received injected ping key\n";
        }
    }
    Unhook(hook_handle);
}

HHOOK KeyboardHook::Hook() {
    HookStatus expected = HookStatus::Uninitialized;
    if (!hook_status_.compare_exchange_strong(expected, HookStatus::Active)) {
        std::cerr << "[WARNING] KeyboardHook worker attempted to spawn second hook!\n";
        if (expected == HookStatus::Active) {
            std::cerr << "    hook claimed by another instance/thread\n";
        }
        else {
            std::cerr << "    hook is in error state\n";
        }
        return NULL;
    }
    HHOOK handle = SetWindowsHookEx(WH_KEYBOARD_LL, ProcessKeys, GetModuleHandle(NULL), 0);
    if (handle == NULL) {
        hook_status_.store(HookStatus::Uninitialized);
        DWORD response = GetLastError();
        std::cerr << std::format("[ERROR] KeyboardHook worker failed to create hook handle with "
                                 "error: {}\n", response);
    }
    return handle;
}

bool KeyboardHook::Unhook(HHOOK handle) {
    HookStatus hookstatus = hook_status_.load();
    if (hookstatus != HookStatus::Active) {
        std::cerr << std::format("[WARNING] KeyboardHook worker attempted to unhook {} hook!\n",
                                 (hookstatus == HookStatus::Error) ? "broken" : "uninitialized");
        return false;
    }
    bool success = UnhookWindowsHookEx(handle);
    if (!success) {
        DWORD response = GetLastError();
        std::cerr << std::format("[ERROR] KeyboardHook failed to unhook with error: {}\n", response);
        std::cerr << "    KeyboardHook will remain occupied to prevent stale hooks\n";
        hook_status_.store(HookStatus::Error); //do not restore! signal to resolve stale hooks 
        return false;
    }
    hook_status_.store(HookStatus::Uninitialized);
    return true;
}
