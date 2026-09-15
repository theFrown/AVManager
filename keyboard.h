#pragma once

#include <atomic>
#include <thread>
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
    KeyboardHook() = default;
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook(KeyboardHook&&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;
    KeyboardHook& operator=(KeyboardHook&&) = delete;

    enum class WorkerStatus { PreStart, Running, QuitRequest, QuitMessageError, QuitHookError };
    enum class HookStatus { Uninitialized, Active, Error};

    inline static KeyCombos raw_keys = {};

    static LRESULT CALLBACK ProcessKeys(int nCode, WPARAM wParam, LPARAM lParam);
    static bool PingSend();
    static bool PingReply();
    bool Start();
    bool Stop(bool force = false);
    WorkerStatus GetWorkerStatus() const { return worker_status_.load(); }
    HookStatus GetHookStatus() const { return hook_status_.load(); }

private:
    constexpr static ULONG_PTR ping_key_signature_ = 531764;
    constexpr static int ping_message_number_ = 1;
    inline static std::atomic<HookStatus> hook_status_ = HookStatus::Uninitialized;
    
    std::thread worker_;
    std::atomic<DWORD> worker_thread_id_ = 0;
    std::atomic<WorkerStatus> worker_status_ = WorkerStatus::PreStart;

    void Run();
    HHOOK Hook();
    bool Unhook(HHOOK handle);
};
