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
        bool keydown = (wParam == WM_KEYDOWN) || (wParam == WM_SYSKEYDOWN);
        
        switch (key_message.vkCode) {       // phase 1: handle any modifier
            case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL:
                ctrl_down_.store(keydown);
                return CallNextHookEx(0, nCode, wParam, lParam);
            case VK_LMENU: case VK_RMENU: case VK_MENU:
                alt_down_.store(keydown);
                return CallNextHookEx(0, nCode, wParam, lParam);
            case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT:
                shift_down_.store(keydown);
                return CallNextHookEx(0, nCode, wParam, lParam);
        }

        bool altdown = (wParam == WM_SYSKEYDOWN) || (wParam == WM_SYSKEYUP);
        bool ctrldown = ctrl_down_.load();
        if (ctrldown) altdown = alt_down_.load();
        else alt_down_.store(altdown);
        bool shiftdown = shift_down_.load();
        bool ping_signed = key_message.dwExtraInfo == ping_key_signature_;

        // v v debug only, remove me
        // bool injected = key_message.flags & LLKHF_INJECTED; //might remove now we have dwExtraInfo
        // std::cout << key_message.time << ": " << key_message.vkCode;
        // std::cout << ((injected) ? ", injected" : ", organic");
        // switch (wParam) { //this whole switch case can go once we stop using cout
        //     case WM_KEYDOWN:
        //         std::cout << ", normal, down\n";
        //         break;
        //     case WM_KEYUP:
        //         std::cout << ", normal, up\n";
        //         break;
        //     case WM_SYSKEYDOWN:
        //         std::cout << ", system, down\n";
        //         break;
        //     case WM_SYSKEYUP:
        //         std::cout << ", system, up\n";
        //         break;
        //     default:
        //         std::cerr << "\n[WARNING] KeyboardHook unknown wParam: " << wParam << '\n';
        //         break;
        // }
        // ^ ^ debug only, remove me

        switch (key_message.vkCode) {      //phase 2: handle any other key
            case VK_END:  //TODO: consider removing/remapping once everything is stable
                if (keydown && altdown && ctrldown && !shiftdown) {
                    PostQuitMessage(0);
                }
                if (keydown && ctrldown && shiftdown && !altdown) {
                    PingSend();
                }
                break;
            case VK_VOLUME_UP:
                if (ProcessCombo(raw_keys.vup, keydown, ctrldown, altdown, shiftdown)) {
                    return swallow_key_value_;
                }
                break;
            case VK_VOLUME_DOWN:
                if (ProcessCombo(raw_keys.vdown, keydown, ctrldown, altdown, shiftdown)) {
                    return swallow_key_value_;
                }
                break;
            case VK_VOLUME_MUTE:
                if (!keydown && ping_signed) {
                    PingRelay();
                    return swallow_key_value_;
                }
                else if (!shiftdown) {
                    if (keydown) {
                        raw_keys.mute.counter++;
                        raw_keys.mute.isdown.store(true);
                    }
                    else {
                        raw_keys.mute.isdown.store(false);
                    }
                    return swallow_key_value_;
                }
                break;
        }
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

bool KeyboardHook::ProcessCombo(KeyStates::Combos& keycombo, bool keydown, bool ctrldown,
                                bool altdown, bool shiftdown) {
    if (ctrldown && !shiftdown && !altdown) {
        if (keydown) {
            keycombo.ctrl.counter++;
            keycombo.ctrl.isdown.store(true);
        }
        else {
            keycombo.ctrl.isdown.store(false);
        }
        return true;
    }
    if (altdown && !ctrldown && !shiftdown) {
        if (keydown) {
            keycombo.alt.counter++;
            keycombo.alt.isdown.store(true);
        }
        else {
            keycombo.alt.isdown.store(false);
        }
        return true;
    }
    if (shiftdown && !ctrldown && !altdown) {
        return false;
    }
    // v v any time zero or multiple modifiers are down
    if (keydown) {
        keycombo.bare.counter++;
        keycombo.bare.isdown.store(true);
    }
    else {
        keycombo.bare.isdown.store(false);
    }
    return true;
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
    input.ki.wVk = VK_VOLUME_MUTE;
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

bool KeyboardHook::PingRelay() {
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
            std::cout << "KeyboardHook received injected ping key\n"; //TODO: replace report mechanism
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

void KeyboardHook::PrintStates() const {
    const KeyStates& keystates = raw_keys;
    std::cout << "     |    vup    |   vdown   |   mute    |\n";
    std::cout << "     | dwn - cnt | dwn - cnt | dwn - cnt |\n";
    std::cout << std::format("bare |  {:d}  - {: >3} |  {:d}  - {: >3} |  {:d}  - {: >3} |\n", 
                             keystates.vup.bare.isdown.load(), keystates.vup.bare.counter.load(),
                             keystates.vdown.bare.isdown.load(), keystates.vdown.bare.counter.load(),
                             keystates.mute.isdown.load(), keystates.mute.counter.load());
    std::cout << std::format("ctrl |  {:d}  - {: >3} |  {:d}  - {: >3} |     -     |\n", 
                             keystates.vup.ctrl.isdown.load(), keystates.vup.ctrl.counter.load(),
                             keystates.vdown.ctrl.isdown.load(), keystates.vdown.ctrl.counter.load());
    std::cout << std::format("alt  |  {:d}  - {: >3} |  {:d}  - {: >3} |     -     |\n", 
                             keystates.vup.alt.isdown.load(), keystates.vup.alt.counter.load(),
                             keystates.vdown.alt.isdown.load(), keystates.vdown.alt.counter.load());
}
