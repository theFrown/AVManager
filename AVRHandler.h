#pragma once

#include <iostream>
#include <array>
#include <string>
#include <optional>
#include <winsock2.h>
#include "timing.h"

struct KeyStates; //defined in keyboard.h, pulled in by AVRHandler.cpp before SyncKeys uses it

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
        if (original_slot_ >= Verbosity::Info) {
            std::cout << "Restoring verbosity level\n"; 
        }
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
    std::string mute;
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

    enum class ExitCode { Normal, Ping, Sync, Key, Receive, Send, Resync };
    enum class CtorCode { Healthy, Unknown, SocketSetupFailed, ConnectionFailed };

    bool stayalive = false;

    ExitCode ControlLoop();
    CtorCode GetCtorStatus() const { return ctor_status_; }

    //helper functions, public only so they can be externally tested
    static std::string MakeCommand(std::string cmd, std::optional<int> num = std::nullopt);
    static std::string DbToString(int db_tenths, int zero = 800, int min = 0, int max = 980);
    static std::optional<int> StringToDb(std::string_view str, int zero = 800);
    static std::string PrintDb(int value, int width = 0);

private:
    enum class Report_ { OK, Data, NoData, Wait, Unknown, BadInput, Disconnected, SocketError, 
                         KeyError };
    enum class Waker_ { ComsReady, WindowMessageIn, TimeOut, Failed, Unknown };
    enum class ControlMode_ { Request, Report, Rest };
    
    constexpr static const char* ip_string_ = "192.168.1.200";
    constexpr static int port_ = 23;
    constexpr static size_t inbufferlen_ = 270; //2x denon message max size (135 chars)
    constexpr static int heartbeat_interval_ = 1000;
    constexpr static int min_volume = -800; //in 0.1db
    constexpr static int min_chanvol = -120;
    constexpr static int max_chanvol = 120;
    constexpr static int max_volume_increase_steps = 10; //large volume jumps hurt ears and speakers
    constexpr static int key_sync_warn_delay_ = 100; //below is probably hard to notice in practice
    constexpr static int key_sync_error_delay_ = static_cast<int>(1.1 * heartbeat_interval_);
    int volume_step_ = 5; //the only one that can be changed in the denon
    Timer command_cooldown_{50};
    Timer response_deadline_{200}; //default should be longer than command_cooldown_'s!
    
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
    DenonState requested_;
    DenonState commanded_;
    DenonState reported_;
    int failed_syncs_ = 0;
    bool connection_healthy_ = false;
    bool connection_shutting_down_ = false;
    bool event_healthy_ = false;
    CtorCode ctor_status_ = CtorCode::Unknown;

    Report_ ControlPing(bool silent = true, bool block = true, int time_out = 1000);
    Report_ ControlResync();
    Report_ ControlReceive(int time_out_ms = 0);
    Waker_  ControlSleep(int ms);
    Report_ SyncKeyHook(bool flush = false);
    int     SyncKeys(KeyStates&);
    bool    SyncVolumeKey(int steps, int& stored_value, int min, int max);
    Report_ SyncIn();
    Report_ SyncInChanvols(std::string_view message);
    Report_ SyncInString(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, std::string& report_slot);
    Report_ SyncInDb(std::string_view message, std::string_view prefix, 
                      std::string_view report_string, int& report_slot);
    Report_ SyncOut();
    Report_ SyncOutChanvols();
    Report_ SyncOutDb(const std::string& prefix, int value, int& slot);
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
