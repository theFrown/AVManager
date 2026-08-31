#pragma once

#include <iostream>
#include <array>
#include <string>
#include <chrono>
#include <thread>
#include <optional>
#include <winsock2.h>

enum class Verbosity { Silent, Error, Warning, Info, Debug, Trace};

class VerbosityOverride {
private:
    Verbosity& original_slot_;
    Verbosity original_value_;
public:
    VerbosityOverride(Verbosity& original, Verbosity temp_level) 
        : original_slot_(original), original_value_(original) 
    {
        if (temp_level >= Verbosity::Info) {
            std::cout << "Temporarily overriding verbosity level\n"; 
        }
        original_slot_ = temp_level; 
    }
    ~VerbosityOverride() { 
        original_slot_ = original_value_; 
    }
    VerbosityOverride(const VerbosityOverride&) = delete;
    VerbosityOverride(VerbosityOverride&&) = delete;
    VerbosityOverride& operator=(const VerbosityOverride&) = delete;
    VerbosityOverride& operator=(VerbosityOverride&&) = delete;
};

class Timer {
private:
    using Clock_ = std::chrono::steady_clock;
    int default_period_;
    std::optional<Clock_::time_point> deadline_;

public:
    Timer(int ms = 0, bool set_now = false) : default_period_(ms) { 
        if (set_now) Set(ms);
    }

    void Set(int ms) { deadline_ = Clock_::now() + std::chrono::milliseconds(ms); }
    void Set() { Set(default_period_); }
    void Reset() { deadline_.reset(); }
    void SetDefault(int ms) { default_period_ = ms; }
    void Wait() const { if (IsPending()) std::this_thread::sleep_until(*deadline_); }
    bool IsSet() const { return deadline_.has_value(); }
    bool IsExpired() const { return deadline_.has_value() && (Clock_::now() > *deadline_); }
    bool IsPending() const { return IsSet() && !IsExpired(); }
};

class Stopwatch {
private:
    using Clock_ = std::chrono::steady_clock;
    using MilliSeconds_ = std::chrono::milliseconds;
    std::optional<Clock_::time_point> start_;
    std::optional<Clock_::time_point> stop_;

public:
    Stopwatch(bool start = true) { if (start) Start(); }
    
    void Start() { 
        if (start_.has_value() && stop_.has_value()) {
            start_ = Clock_::now() - *stop_ + *start_;
        }
        else {
            start_ = Clock_::now(); 
        }
        stop_.reset();
    }
    void Stop() { if (start_.has_value()) stop_ = Clock_::now(); }
    void Reset() { 
        start_.reset();
        stop_.reset(); 
    }
    std::optional<int> Read() const { 
        if (!start_.has_value()) return std::nullopt;
        auto delta = stop_.has_value() ? (*stop_ - *start_) : (Clock_::now() - *start_);
        return static_cast<int>(std::chrono::duration_cast<MilliSeconds_>(delta).count()); 
    }
    void Print() const {
        auto value = Read();
        if (value.has_value()) {
            std::cout << "Time elapsed: " << *value << " ms\n";
        }
        else {
            std::cout << "stopwatch wasn't running!\n";
        }
    }
};

struct DenonState {
    std::string power;
    std::string input;
    std::string surround;
    int volume = 0; //all volumes stored in 0.1db increments: 10dB stored as 100, 10.5db as 105
    int maxvolume = 0;
    struct ChanVol { //volumes stored as offsets relative to volume
        int FL = 0;
        int FR = 0;
        int C =  0;
        int SW = 0;
        int SL = 0;
        int SR = 0;
        bool operator==(const ChanVol&) const = default;
    } chanvol;
    bool operator==(const DenonState&) const = default;
};

class AVRHandler {
public:
    AVRHandler(Verbosity verbosity = Verbosity::Info);
    ~AVRHandler();
    AVRHandler(const AVRHandler&) = delete;
    AVRHandler(AVRHandler&&) = delete;
    AVRHandler& operator=(const AVRHandler&) = delete;
    AVRHandler& operator=(AVRHandler&&) = delete;

    bool stayalive = false;
    int status = -1;

    int ControlLoop();

    //helper functions, public only so they can be externally tested
    static std::string MakeCommand(std::string cmd, std::optional<int> num = std::nullopt);
    static std::string DbToString(int db_tenths, int zero = 800, int min = 0, int max = 980);
    static std::optional<int> StringToDb(std::string_view str, int zero = 800);
    static std::string PrintDb(int value, int width = 0);

private:
    enum class Report_ { OK, Data, NoData, Wait, Unknown, BadInput, Disconnected, SocketError };
    enum class Waker_ { ComsReady, WindowMessageIn, TimeOut, Failed, Unknown};
    enum class ControlMode_ { Request, Report, Rest };
    
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static size_t inbufferlen_ = 270; //2x denon message max size (135 chars)
    Timer command_cooldown_{50};
    Timer response_deadline_{200};
    
    std::array<char, inbufferlen_ + 1> inbuffer_ = {};
    std::string inchain_;
    std::string inmessage_;
    std::string signal_;
    bool signal_received_ = false;
    SOCKET socket_ = INVALID_SOCKET;
    WSAEVENT socket_event_ = WSA_INVALID_EVENT;
    sockaddr_in sockaddr_ = {};
    Verbosity verbosity_;   //constructor initializer list
    ControlMode_ control_mode_ = ControlMode_::Report;
    DenonState requested_ = {};
    DenonState commanded_ = {};
    DenonState reported_ = {};
    int failed_syncs_ = 0;
    bool connection_healthy_ = false;
    bool connection_shutting_down_ = false;
    bool event_healthy_ = false;

    Report_ ControlPing(bool silent = true, bool block = true, int time_out = 1000);
    Report_ ControlResync();
    Report_ ControlReceive(int time_out_ms = 0);
    Waker_  ControlSleep(int ms);
    Report_ SyncIn();
    Report_ SyncInString(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, std::string& report_slot);
    Report_ SyncInDb(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, int& report_slot);
    Report_ SyncOut();
    Report_ SyncResolve();
    Report_ SetupSocket();
    Report_ Connect();
    Report_ Receive();
    Report_ Parse();
    Report_ Send(std::string_view cmd, bool wait = false);
    
    void Print(Verbosity level, std::string_view msg) const;
    void Print(std::string_view msg) const { Print(Verbosity::Info, msg); }
    void PrintStates(Verbosity level);
};