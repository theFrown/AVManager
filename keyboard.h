#pragma once

#include <atomic>
#include <thread>
#include <windows.h> //lean + nomin in CMakeLists

struct KeyStates {
    struct State {
        std::atomic<bool> isdown = false;
        std::atomic<int> counter = 0;
    };
    struct Combos {
        State bare;
        State ctrl;
        State alt;
    };
    Combos vup;
    Combos vdown;
    State mute;
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
    enum class PingStatus { Unknown, Sent, SendFailed, Received };

    inline static KeyStates raw_keys = {};

    bool Start();
    bool Stop(bool force = false);
    static LRESULT CALLBACK ProcessKeys(int nCode, WPARAM wParam, LPARAM lParam);
    static bool PingKey();
    bool PingMsg();
    void PrintStates() const; 
    WorkerStatus GetWorkerStatus() const { return worker_status_.load(); }
    HookStatus GetHookStatus() const { return hook_status_.load(); }
    PingStatus GetPingKeyStatus() const { return ping_key_status_.load(); }
    PingStatus GetClearPingKeyStatus() { return ping_key_status_.exchange(PingStatus::Unknown); }
    PingStatus GetPingMsgStatus() const { return ping_msg_status_.load(); }
    PingStatus GetClearPingMsgStatus() { return ping_msg_status_.exchange(PingStatus::Unknown); }

private:
    constexpr static LRESULT swallow_key_value_ = 1;
    constexpr static ULONG_PTR ping_key_signature_ = 531764;
    constexpr static int ping_message_number_ = 1;
    inline static std::atomic<PingStatus> ping_key_status_ = PingStatus::Unknown;
    inline static std::atomic<HookStatus> hook_status_ = HookStatus::Uninitialized;
    inline static std::atomic<bool> ctrl_down_ = false;
    inline static std::atomic<bool> shift_down_ = false;
    inline static std::atomic<bool> alt_down_ = false;

    std::thread worker_;
    std::atomic<DWORD> worker_thread_id_ = 0;
    std::atomic<WorkerStatus> worker_status_ = WorkerStatus::PreStart;
    std::atomic<PingStatus> ping_msg_status_ = PingStatus::Unknown;

    void Run();
    static HHOOK Hook();
    static bool Unhook(HHOOK handle);
    static bool ProcessCombo(KeyStates::Combos& keycombo, bool keydown, bool ctrldown, bool altdown,
                             bool shiftdown);
};
