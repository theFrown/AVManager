#pragma once

#include <iostream>
#include <array>
#include <string>
#include <chrono>
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

struct DenonState {
    std::string power;
    std::string input;
    std::string surround;
    int volume; //all volumes stored in 0.1db increments: 10dB stored as 100, 10.5db as 105
    int maxvolume;
    struct ChanVol {
        int FL;
        int FR;
        int C;
        int SW;
        int SL;
        int SR;
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
    int status;

    int ControlLoop();
    static std::string dbtostring(int db_tenths, int zero = 800, int min = 0, int max = 980);
    static std::optional<int> stringtodb(std::string_view str, int zero = 800);
    static std::string printdb(int value, int width = 0);

private:
    enum class Report_ { OK, Data, NoData, Wait, Unknown, BadInput, Disconnected, SocketError };
    enum class ControlMode_ { Request, Report, Rest };
    using Clock_ = std::chrono::steady_clock;
    using TimePoint_ = Clock_::time_point;
    using MilliSeconds_ = std::chrono::milliseconds;
    
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static size_t inbufferlen_ = 270; //2x denon message max size (135 chars)
    const MilliSeconds_ cooldown_default_{50};
    const MilliSeconds_ patience_default_{200};
    
    std::array<char, inbufferlen_ + 1> inbuffer_;
    std::string inchain_;
    std::string inmessage_;
    std::string_view signal_;
    bool signal_received_ = false;
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in sockaddr_;
    Verbosity verbosity_;
    ControlMode_ control_mode_ = ControlMode_::Report;
    DenonState requested_ = {};
    DenonState commanded_ = {};
    DenonState reported_ = {};
    int failed_syncs_ = 0;
    bool connection_healthy_ = false;
    bool connection_shutting_down_ = false;
    TimePoint_ command_cooldown_;
    TimePoint_ response_deadline_;

    Report_ ControlResync();
    Report_ ControlReceive(int time_out = 0);
    Report_ ControlPing(bool silent = true, bool block = true, int time_out = 1000);
    Report_ SyncIn();
    Report_ SyncInString(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, std::string& report_slot);
    Report_ SyncInDb(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, int& report_slot);
    Report_ SyncOut();
    Report_ SyncResolve();
    Report_ SetupSocket();
    Report_ Connect();
    Report_ CheckIncoming(int time_out = 0);
    Report_ Receive();
    Report_ Parse();
    Report_ Send(std::string_view cmd, bool wait = false);
    std::string MakeCommand(std::string cmd, std::optional<int> num = std::nullopt) const;
    void Print(Verbosity level, std::string_view msg) const;
    void Print(std::string_view msg) const { Print(Verbosity::Info, msg); }
    void PrintStates(Verbosity level);
};