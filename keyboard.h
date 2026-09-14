#pragma once

#include <atomic>
#include <windows.h> //lean + nomin in CMakeLists

struct KeyCombos {
    struct States {
        std::atomic<bool> isdown = false;
        std::atomic<int> counter = 0;
    };
    States vup;
    States vdown;
    States mute;
    States ctrl_vup;
    States ctrl_vdown;
    States alt_vup;
    States alt_vdown;
};

class KeyboardHook {
public:
    KeyboardHook();
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook(KeyboardHook&&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;
    KeyboardHook& operator=(KeyboardHook&&) = delete;

    int status = -1;
    inline static KeyCombos raw_keys = {};

    static LRESULT CALLBACK ProcessKeys(int nCode, WPARAM wParam, LPARAM lParam);
    static bool PingSend();
    static bool PingReply();
    int Run();
    // bool Stop();
    // bool IsRunningAndHealthy();

private:
    constexpr static ULONG_PTR ping_key_signature_ = 5317;
    constexpr static int ping_message_number_ = 1;
    // bool hook_active_ = false;
    HHOOK hook_handle_ = NULL;
    inline static std::atomic<bool> hook_occupied_ = false;
};
