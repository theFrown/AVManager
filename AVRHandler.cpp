#include <iostream>
#include <optional>
#include <stdexcept>
#include <format>
#include <string>
#include <cmath>
#include <utility>
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>    //lean + nomin in CMakeLists
#include "AVRHandler.h"
#include "DenonProtocol.h"
#include "keyboard.h"
#include "timing.h"

AVRHandler::AVRHandler(Verbosity verbosity) 
    : verbosity_(verbosity)
{   
    Report_ response = SetupSocket();
    if (response != Report_::OK) {
        ctor_status_ = CtorCode::SocketSetupFailed;
    }
    else {
        response = Connect();
        if (response != Report_::OK) {
            ctor_status_ = CtorCode::ConnectionFailed;
        }
        else {
            ctor_status_ = CtorCode::Healthy;
            stayalive = true;
        }
    }
}

AVRHandler::~AVRHandler() {
    Print("Shutting down AVRHandler\n");
    if (socket_ != INVALID_SOCKET) {
        int response = shutdown(socket_, SD_SEND);
        if (response == 0) {
            Timer kill_deadline{2000, true};
            Report_ coms_response = Report_::Unknown;
            bool listening = true;
            connection_shutting_down_ = true;
            while (listening) {
                coms_response = ControlReceive(*kill_deadline.GetRemaining());
                if (coms_response == Report_::Disconnected) {
                    Print("Connection shutdown confirmed\n");
                    listening = false;
                }
                else if (coms_response == Report_::SocketError) {
                    Print("Connection error, forcing shutdown\n"); 
                    listening = false;
                }
                if (kill_deadline.IsExpired()) {
                    Print("Connection timed out, forcing shutdown\n");
                    listening = false;
                }
            }
        }
        else {
            if (response == SOCKET_ERROR) {
                response = WSAGetLastError();
            }
            Print(Verbosity::Warning, std::format("Socket shutdown failed with error: {}\n"
                                                  "    proceeding with hard shutdown\n", response));
            linger value{.l_onoff = 1, .l_linger = 0};
            const char* value_byteaddress = reinterpret_cast<const char*>(&value);
            setsockopt(socket_, SOL_SOCKET, SO_LINGER, value_byteaddress, sizeof(value));
        }
        closesocket(socket_);
    }
    if (socket_event_ != WSA_INVALID_EVENT) {
        WSACloseEvent(socket_event_);
    }
    WSACleanup();
}

auto AVRHandler::ControlLoop() -> ExitCode {
    KeyboardHook::KeyListener key_listener;
    Report_ response = ControlPing(false);
    if (response != Report_::OK) {
        return ExitCode::Ping;
    }
    response = ControlResync();
    if (response != Report_::OK) {
        return ExitCode::Sync;
    }
    response = SyncKeyHook(true); //flush any keypresses inbound before we were ready
    if (response != Report_::OK) {
        return ExitCode::Key;
    }
    SyncResolve(); //synchronize the 3 states

    while (stayalive) {
        //reading keys: making sure requested_ is up-to-date through SyncKeys
        response = SyncKeyHook();
        if (response == Report_::KeyError) {
            return ExitCode::Key;
        }
        else if (response == Report_::Disconnected) {
            return ExitCode::Commanded;
        }

        //receiving: making sure reported_ is up-to-date through SyncIn
        response = ControlReceive();
        if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
            return ExitCode::Receive;
        }

        //sending: making sure commanded_ is up-to-date through SyncOut
        response = SyncOut();
        if ((response == Report_::SocketError) || (response == Report_::BadInput)) {
            return ExitCode::Send;
        }

        //closing the loop: making sure commanded_ and reported_ reconcile
        if (control_mode_ != ControlMode_::Rest) { 
            response = SyncResolve();
            if (response == Report_::Unknown) {
                response = ControlResync();
                if (response != Report_::OK) {
                    return ExitCode::Resync;
                }
            }
        }

        //sleeping until earliest event or nearest deadline
        int deadline1 = command_cooldown_.GetRemaining().value_or(heartbeat_interval_);
        int deadline2 = response_deadline_.GetRemaining().value_or(heartbeat_interval_);
        ControlSleep(std::min({deadline1, deadline2, heartbeat_interval_}));
    }
    return ExitCode::Normal;
}

auto AVRHandler::ControlPing(bool silent, bool block, int time_out) -> Report_ {
    if (!block) {
        Print(Verbosity::Warning, "non-blocking ControlPing still needs to be implemented!\n");
        block = true; //TODO: implement and remove this guard!
    }
    std::optional<VerbosityOverride> silence;
    if (silent) {
        silence.emplace(verbosity_, Verbosity::Warning);
    }
    Print(Verbosity::Debug, "Testing coms channel\n");
    signal_ = denon_cmd::power_prefix;
    signal_received_ = false;
    Report_ response = Send(denon_cmd::power_status, block);
    Timer deadline;
    if ((!block) || (response != Report_::OK)) {
        return response;
    }
    else {
        if (time_out > 0) {
            Print(Verbosity::Debug, std::format("Waiting until we receive expected response (up to "
                                                "{} ms)\n", time_out));
            deadline.Set(time_out);
        }
        else {
            Print(Verbosity::Debug, "Waiting until we receive expected response\n");
        }
        while (!signal_received_) {
            if (deadline.IsExpired()) {
                Print(Verbosity::Warning, "Polling the AVR timed out\n");
                return Report_::NoData; 
            }
            int wait = deadline.GetRemaining().value_or(500);
            response = ControlReceive(wait);
            if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
                return response;
            }
        }
        Print(Verbosity::Debug, "AVR connection confirmed\n");
        return Report_::OK;
    }
}

auto AVRHandler::ControlResync() -> Report_ {
    Print("Syncing all parameters from the AVR\n");
    PrintStates(Verbosity::Debug);
    Timer deadline;
    Report_ response = Report_::Unknown;
    int attempt = 1;
    bool sent = false;
    bool succeeded = false;
    //TODO: might be smart to sleep for 'patience' ms (or more) to purge denon incoming messages 
    for (int stage = 0; stage < 6; stage++) {
        attempt = 1;
        sent = false;
        succeeded = false;
        while (attempt < 4) {
            if (!sent) {
                switch (stage) {
                    case 0:
                        Print(Verbosity::Debug, "Syncing power state\n");
                        response = Send(denon_cmd::power_status, true);
                        signal_ = denon_cmd::power_prefix; //read as 'last message we care about'
                        break;                             //more may follow but will be ignored
                    case 1:
                        Print(Verbosity::Debug, "Syncing input/source\n");
                        response = Send(denon_cmd::input_status, true);
                        signal_ = denon_cmd::input_prefix;
                        break;
                    case 2:
                        Print(Verbosity::Debug, "Syncing surround mode\n");
                        response = Send(denon_cmd::surround_status, true);
                        signal_ = denon_cmd::surround_prefix;
                        break;
                    case 3:
                        Print(Verbosity::Debug, "Syncing mute state\n");
                        response = Send(denon_cmd::mute_status, true);
                        signal_ = denon_cmd::mute_prefix;
                        break;
                    case 4:
                        Print(Verbosity::Debug, "Syncing master volume\n");
                        response = Send(denon_cmd::volume_status, true);
                        signal_ = denon_cmd::volume_maxprefix;
                        break;
                    case 5:
                        Print(Verbosity::Debug, "Syncing channel volumes\n");
                        response = Send(denon_cmd::chanvol_status, true);
                        signal_ = denon_cmd::chanvol_endreport;
                        break;
                    default:
                        Print(Verbosity::Error, "ControlResync hit undefined stage\n");
                        return Report_::Unknown;
                        break;
                }
                if (response == Report_::OK) {
                    sent = true;
                    signal_received_ = false;
                    deadline.Set(1000);
                }
                else {
                    return response;
                }
            }
            else {
                response = ControlReceive(500);
                if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
                    return response;
                }
                if (signal_received_) {
                    Print(Verbosity::Trace, "Parameter successfully synced\n");
                    succeeded = true;
                    break;
                }
                else if (deadline.IsExpired()) {
                    Print(Verbosity::Warning, std::format("Sync stage {} timed out\n", stage)); 
                    sent = false;
                    attempt++;
                }
            }
        }
        if (!succeeded) {
            Print(Verbosity::Warning, std::format("Resync failed, exceeded retry attempts for stage"
                                                  " {}\n", stage));
            return Report_::Unknown; //TODO: see if it's worth adding a 'timeout' return value
        }
    }
    Print(Verbosity::Info, "Full resync successful, state parameters now:\n");
    PrintStates(Verbosity::Info);
    return Report_::OK;
}

auto AVRHandler::ControlReceive(int time_out_ms) -> Report_ {
    if (time_out_ms > 0) {
        ControlSleep(time_out_ms);
    }
    Report_ response = Receive();
    if ((response == Report_::SocketError) || (response == Report_::Disconnected)) return response;
    if (inchain_.empty()) return response; //draining inchain_ is more important than transparency
    while ((response = Parse()) != Report_::NoData) { //i.e. inchain_ holds something worth parsing
        if (!inmessage_.empty()) {
          response = SyncIn();
          if (inchain_.empty() && response != Report_::OK) return response;
        }
    }
    return Report_::OK;
}

auto AVRHandler::ControlSleep(int ms) -> Waker_ { //TODO: consider intentionally ignoring coms case
    DWORD response = 0;
    if (event_healthy_) {
        response = MsgWaitForMultipleObjects(1, &socket_event_, FALSE, ms, QS_POSTMESSAGE);
    }
    else {
        response = MsgWaitForMultipleObjects(0, nullptr, FALSE, ms, QS_POSTMESSAGE);
        if (response == WAIT_OBJECT_0) {
            response++; //remaps window message to the same switch case below
        }
    }
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0); //TODO: parse thread messages
    switch (response) {
        case WAIT_OBJECT_0:
            Print(Verbosity::Trace, "Woken by coms being ready\n");
            return Waker_::ComsReady;
        case (WAIT_OBJECT_0 + 1):
            Print(Verbosity::Trace, "Woken by window message\n");
            return Waker_::WindowMessageIn;
        case WAIT_TIMEOUT:
            Print(Verbosity::Trace, "Woken by time-out\n");
            return Waker_::TimeOut;
        case WAIT_FAILED: {
            DWORD error = GetLastError();
            Print(Verbosity::Error, std::format("Sleep function failed with error {}\n", error));
            return Waker_::Failed;
        }
        default:
            Print(Verbosity::Error, std::format("Sleep function failed for unknown reason with "
                                                "response: {}\n", response));
            return Waker_::Unknown;
    }
}

auto AVRHandler::SyncKeyHook(bool flush) -> Report_ {
    KeyStates& keys = KeyboardHook::raw_keys;
    if (keys.quit.exchange(false)) {
        Print("Shutdown command received\n");
        return Report_::Disconnected;
    }
    auto error = KeyboardHook::GetError(true);
    if (error.has_value()) {
        switch (*error) {
            case ERROR_INVALID_THREAD_ID:
                Print(Verbosity::Error, "KeyboardHook failed to call thread message, reports "
                                        "Invalid ID\n");
                break;
            case ERROR_NOT_ENOUGH_QUOTA:
                Print(Verbosity::Error, "KeyboardHook failed to call thread message, reports "
                                        "Queue Full\n");
                break;
            case ERROR_ACCESS_DENIED:
                Print(Verbosity::Error, "KeyboardHook failed to call thread message, reports "
                                        "Access Denied\n");
                break;
            default:
                Print(Verbosity::Error, std::format("KeyboardHook failed to call thread message "
                                                    "with unknown error: {}\n", *error));
        }
    }
    DenonState backup = requested_;
    int response = SyncKeys(keys);
    std::optional<int> delay = keys.processing_delay.Read(true);
    if (flush) {
        requested_ = backup;
        return Report_::OK;
    }
    if (response > 0) control_mode_ = ControlMode_::Request;
    if (delay.has_value()) {
        if (*delay > key_sync_error_delay_) {
            Print(Verbosity::Error, std::format("Key stroke processing delay exceeded critical "
                                                "threshold! delay was {}\n", *delay));
            return Report_::KeyError;
        }
        if (*delay > key_sync_warn_delay_) {
            Print(Verbosity::Warning, std::format("Key stroke processing delay exceeded perception "
                                                  "threshold! delay was {}\n", *delay)); 
        }
        else {
            Print(Verbosity::Debug, std::format("Key stroke processing delay was {}\n", *delay));
        }
    }
    return Report_::OK; //only reports success, control_mode_ handles whether there was input
}

int AVRHandler::SyncKeys(KeyStates& keys) {
    int requests = 0;
    int steps = 0;
    
    steps = keys.mute.counter.exchange(0);
    if (steps) {
        std::string requested;
        if (steps > 1) {
            requested = "ON"; //safest interpretation if someone is holding this key down
        }
        else {
            requested = requested_.mute == "ON" ? "OFF" : "ON"; //activate when stored mute is empty
        }
        if (requested != requested_.mute) {
            requests++;
            requested_.mute = requested;
        }
    }

    steps = keys.vup.bare.counter.exchange(0) - keys.vdown.bare.counter.exchange(0);
    if (steps > max_volume_increase_steps_) steps = max_volume_increase_steps_;
    requests += SyncVolumeKey(steps, requested_.volume, min_volume_, reported_.maxvolume);

    steps = keys.vup.ctrl.counter.exchange(0) - keys.vdown.ctrl.counter.exchange(0);
    requests += SyncVolumeKey(steps, requested_.chanvol.C, min_chanvol_, max_chanvol_);

    steps = keys.vup.alt.counter.exchange(0) - keys.vdown.alt.counter.exchange(0);
    requests += SyncVolumeKey(steps, requested_.chanvol.SL, min_chanvol_, max_chanvol_);
    requests += SyncVolumeKey(steps, requested_.chanvol.SR, min_chanvol_, max_chanvol_);
    
    return requests;
}

bool AVRHandler::SyncVolumeKey(int steps, int& stored_value, int min, int max) {
    if (steps) {
        int requested_level = stored_value + steps * volume_step_;
        int clamped_level = std::clamp(requested_level, min, max);
        if (requested_level != clamped_level) MessageBeep(0xFFFFFFFF);
        if (stored_value != clamped_level) {
            stored_value = clamped_level;
            return true;
        }
    }
    return false;
}

auto AVRHandler::SyncIn() -> Report_ {
    if (inmessage_.empty()) {
        Print(Verbosity::Warning, "SyncIn called but no pending message!\n");
        return Report_::NoData;
    }
    std::string message = std::move(inmessage_);
    inmessage_.clear();  //technically probably redundant, but helps me sleep easier
    if (!signal_.empty() && message.starts_with(signal_)) {
        signal_received_ = true;
    }
    if (message.starts_with(denon_cmd::power_prefix)) {
        return SyncInString(message, denon_cmd::power_prefix, "power state", reported_.power);
    }
    else if (message.starts_with(denon_cmd::input_prefix)) {
        return SyncInString(message, denon_cmd::input_prefix, "input source", reported_.input);
    }
    else if (message.starts_with(denon_cmd::surround_prefix)) {
        return SyncInString(message, denon_cmd::surround_prefix, "surround mode", reported_.surround);
    }
    else if (message.starts_with(denon_cmd::mute_prefix)) {
        return SyncInString(message, denon_cmd::mute_prefix, "mute state", reported_.mute);
    }
    else if (message.starts_with(denon_cmd::volume_prefix)) {
        if (message.starts_with(denon_cmd::volume_maxprefix)) {
            return SyncInDb(message, denon_cmd::volume_maxprefix, "max volume", reported_.maxvolume);
        }
        else {
            return SyncInDb(message, denon_cmd::volume_prefix, "volume", reported_.volume);
        }
    }
    else if (message.starts_with(denon_cmd::chanvol_prefix)) {
        return SyncInChanvols(message);
    }
    else {
        Print(Verbosity::Debug, std::format("Unrecognised message [{}]\n", message));
        return Report_::Unknown;
    }
}

auto AVRHandler::SyncInChanvols(std::string_view message) -> Report_ {
    if (message.starts_with(denon_cmd::chanvol_endreport)) {
        Print(Verbosity::Debug, "Channel volume list complete\n");
        return Report_::OK;
    }
    else if (message.starts_with(denon_cmd::chanvol_FL_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_FL_prefix, "FL offset", reported_.chanvol.FL));
    }
    else if (message.starts_with(denon_cmd::chanvol_FR_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_FR_prefix, "FR offset", reported_.chanvol.FR));
    }
    else if (message.starts_with(denon_cmd::chanvol_C_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_C_prefix, "C offset", reported_.chanvol.C));
    }
    else if (message.starts_with(denon_cmd::chanvol_SW_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_SW_prefix, "SW offset", reported_.chanvol.SW));
    }
    else if (message.starts_with(denon_cmd::chanvol_SL_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_SL_prefix, "SL offset", reported_.chanvol.SL));
    }
    else if (message.starts_with(denon_cmd::chanvol_SR_prefix)) {
        return (SyncInDb(message, denon_cmd::chanvol_SR_prefix, "SR offset", reported_.chanvol.SR));
    }
    else {
        auto p = message.substr(std::string_view(denon_cmd::chanvol_prefix).size());
        Print(Verbosity::Warning, std::format("Unknown parameter [{}] for channel volume\n", p));
        return Report_::Unknown;
    }
}

auto AVRHandler::SyncInString(std::string_view message, std::string_view prefix, 
                              std::string_view report_string, std::string& report_slot) -> Report_ {
    std::string_view params = message.substr(prefix.size());
    Print(Verbosity::Debug, std::format("Denon reports {} set to {}\n", report_string, params));
    if (params != report_slot){
        report_slot = params;
        if (control_mode_ != ControlMode_::Request) {
            control_mode_ = ControlMode_::Report;
        }
        PrintStates(Verbosity::Debug);
    }
    else {
        Print(Verbosity::Debug, "Reported value matches internal state, message ignored\n");
    }
    return Report_::OK;
}

auto AVRHandler::SyncInDb(std::string_view message, std::string_view prefix, 
                          std::string_view report_string, int& report_slot) -> Report_ {
    std::string_view params = message.substr(prefix.size());
    std::optional<int> num;
    if (prefix.starts_with(denon_cmd::chanvol_prefix)) {
        num = StringToDb(params, 500);
    }
    else {
        num = StringToDb(params);
    }
    if (!num) {
        Print(Verbosity::Warning, std::format("Parsing incoming message [{}] failed.\n    params [{}]"
                                              " do not convert cleanly to an int\n", prefix, params));
        return Report_::BadInput;
    }
    Print(Verbosity::Debug, std::format("Denon reports {} set to {}\n", report_string, PrintDb(*num)));
    if (*num != report_slot) {
        report_slot = *num;
        if (control_mode_ != ControlMode_::Request) {
            control_mode_ = ControlMode_::Report;
        }
        PrintStates(Verbosity::Debug);
    }
    else {
        Print(Verbosity::Debug, "Reported value matches internal state, message ignored\n");
    }
    return Report_::OK;
}

auto AVRHandler::SyncOut() -> Report_ {
    if (command_cooldown_.IsPending()) return Report_::Wait;
    command_cooldown_.Reset(); //cheaper to always run than checking if it was set
    if (commanded_ == requested_) return Report_::NoData;
    Report_ response = Report_::Unknown;
    if (commanded_.mute != requested_.mute) {
        control_mode_ = ControlMode_::Request;
        response = Send(denon_cmd::mute_prefix + requested_.mute);
        if (response == Report_::OK) {
            commanded_.mute = requested_.mute;
        }
    }
    else if (commanded_.volume != requested_.volume) {
        control_mode_ = ControlMode_::Request;
        response = SyncOutDb(denon_cmd::volume_prefix, requested_.volume, commanded_.volume);
    }
    else if (commanded_.chanvol != requested_.chanvol) {
        control_mode_ = ControlMode_::Request;
        response = SyncOutChanvols();
    }
    else {
        requested_.power = commanded_.power = reported_.power;
        requested_.input = commanded_.input = reported_.input;
        requested_.surround = commanded_.surround = reported_.surround;
        requested_.maxvolume = commanded_.maxvolume = reported_.maxvolume;
        if (commanded_ != requested_) {
            Print(Verbosity::Error, "SyncOut stuck on a parameter that cannot be commanded!\n");
            return Report_::BadInput;
        }
        else {
            Print(Verbosity::Warning, "SyncOut had to force-correct desynced read-only states!\n");
            PrintStates(Verbosity::Debug);
            return Report_::OK;
        }
    }
    PrintStates(Verbosity::Debug);
    if (response == Report_::OK) response_deadline_.Set();
    return response;
}

auto AVRHandler::SyncOutChanvols() -> Report_ {
    Report_ response = Report_::Unknown;
    DenonState::ChanVol& commanded = commanded_.chanvol;
    DenonState::ChanVol& requested = requested_.chanvol;
    if (commanded.FL != requested.FL) {
        response = SyncOutDb(denon_cmd::chanvol_FL_prefix, requested.FL, commanded.FL);
    }
    else if (commanded.FR != requested.FR) {
        response = SyncOutDb(denon_cmd::chanvol_FR_prefix, requested.FR, commanded.FR);
    }
    else if (commanded.C != requested.C) {
        response = SyncOutDb(denon_cmd::chanvol_C_prefix, requested.C, commanded.C);
    }
    else if (commanded.SL != requested.SL) {
        response = SyncOutDb(denon_cmd::chanvol_SL_prefix, requested.SL, commanded.SL);
    }
    else if (commanded.SR != requested.SR) {
        response = SyncOutDb(denon_cmd::chanvol_SR_prefix, requested.SR, commanded.SR);
    }
    else if (commanded.SW != requested.SW) {
        response = SyncOutDb(denon_cmd::chanvol_SW_prefix, requested.SW, commanded.SW);
    }
    return response;
}

auto AVRHandler::SyncOutDb(const std::string& prefix, int value, int& slot) -> Report_ {
    Report_ response = Send(MakeCommand(prefix, value));
    if (response == Report_::OK) {
        slot = value;
    }
    return response;
}

auto AVRHandler::SyncResolve() -> Report_ {
    if (control_mode_ == ControlMode_::Request) {
        if (command_cooldown_.IsPending() || response_deadline_.IsPending()) { 
            return Report_::Wait;
        }
        else {
            if (reported_ == requested_) {
                if (commanded_ == requested_) {
                    Print(Verbosity::Info, "All Commands successfully sent and confirmed\n");
                }
                else { //current loop design should make this impossible
                    Print(Verbosity::Info, "Reported state matches requested state, despite pending"
                                           "commands\n    ignoring unconfirmed commands.\n");
                    commanded_ = requested_;
                }
                failed_syncs_ = 0;
                response_deadline_.Reset();
                control_mode_ = ControlMode_::Rest;
                PrintStates(Verbosity::Debug);
                return Report_::OK;
            }
            else {
                failed_syncs_++;
                if (failed_syncs_ > 5) {
                    Print(Verbosity::Warning, "Sync attempts keep failing, forcing a full Resync "
                                              "from AVR\n");
                    failed_syncs_ = 0;
                    return Report_::Unknown;
                }
                else {
                    Print(Verbosity::Warning, "SyncResolve found one or more parameters in an "
                                              "unexpected state!\n    Forcing SyncOut to resolve.\n");
                    commanded_ = reported_;  //forces desync between requested_ and commanded_
                    PrintStates(Verbosity::Debug);
                    return Report_::Data;
                }
            }
        }
    }
    else {
        if (control_mode_ == ControlMode_::Report) {
            Print(Verbosity::Info, "State successfully updated by AVR-originated change\n");
        }
        else {
            Print(Verbosity::Warning, "SyncResolve called for unknown reasons,\n"
                                      "    harmonizing all states to reported state\n");
        }
        commanded_ = requested_ = reported_;
        failed_syncs_ = 0;
        response_deadline_.Reset();
        control_mode_ = ControlMode_::Rest;
        PrintStates(Verbosity::Debug);
        return Report_::OK;
    }
}

auto AVRHandler::SetupSocket() -> Report_ {
    Print(Verbosity::Info, "Setting up connection\n");
    connection_healthy_ = false;
    WSADATA wsadata;
    int response = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (response != 0) {
        Print(Verbosity::Error, std::format("WSAStartup failed with error:\n    {}\n", response));
        return Report_::SocketError;
    }
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        response = WSAGetLastError();
        Print(Verbosity::Error, std::format("Failed to create socket with error: {}\n", response));
        return Report_::SocketError;
    }
    sockaddr_.sin_family = AF_INET;
    sockaddr_.sin_port   = htons(port_);
    response = inet_pton(AF_INET, ip_string_, &sockaddr_.sin_addr);
    if (response != 1) {
        if (response == 0) {
            Print(Verbosity::Error, "Failed to convert IP address, bad input\n");
        }
        else {
            response = WSAGetLastError();
            Print(Verbosity::Error, std::format("Failed to convert IP address with error: {}\n", 
                                                response));
        }
        return Report_::BadInput;
    }
    socket_event_ = WSACreateEvent();
    if (socket_event_ == WSA_INVALID_EVENT) {
        response = WSAGetLastError();
        Print(Verbosity::Error, std::format("Failed to create socket event with error: {}\n", 
                                            response));
        return Report_::SocketError;
    }   
    return Report_::OK;
}

auto AVRHandler::Connect() -> Report_ {
    Print(Verbosity::Info, "Opening connection\n");
    connection_healthy_ = false;
    int response = connect(socket_, reinterpret_cast<const sockaddr*>(&sockaddr_), sizeof(sockaddr_));
    if (response != 0) {
        if (response == SOCKET_ERROR) {
            response = WSAGetLastError();
        }
        Print(Verbosity::Error, std::format("Failed to connect socket with error: {}\n", 
                                            response));
        return Report_::SocketError;
    }
    response = WSAEventSelect(socket_, socket_event_, FD_READ | FD_CLOSE);
    if (response != 0) {
        if (response == SOCKET_ERROR) {
            response = WSAGetLastError();
        }
        Print(Verbosity::Error, std::format("Failed to bind socket to event with error: {}\n", 
                                             response));
        return Report_::SocketError;
    }
    connection_healthy_ = true; //TODO, consider waiting until confirmed with handshake
    event_healthy_ = true;
    return Report_::OK;
}

auto AVRHandler::Receive() -> Report_ {
    Print(Verbosity::Debug, "Checking for incoming messages\n");
    int response = 0;
    if (event_healthy_) {
        if (!WSAResetEvent(socket_event_)) {
            response = WSAGetLastError();
            Print(Verbosity::Error, std::format("Resetting socket event failed with error: {}\n", 
                                                response));
            event_healthy_ = false;
        }
    }
    response = recv(socket_, inbuffer_.data(), static_cast<int>(inbuffer_.size()), 0);
    if (response == SOCKET_ERROR) {
        response = WSAGetLastError();
        if (response == WSAEWOULDBLOCK) {
            Print(Verbosity::Trace, "No messages available\n");
            return Report_::NoData;
        }
        else {
            Print(Verbosity::Warning, std::format("Failed to retrieve response from AVR with error:"
                                                  " {}\n", response));
            connection_healthy_ = false;
            return Report_::SocketError;
        }
    }
    else if (response == 0) { 
        if (connection_shutting_down_) {
            Print(Verbosity::Debug, "Connection shutdown confirmed by AVR\n");
        }
        else {
            Print(Verbosity::Warning, "Tried to receive from closed connection\n");
        }
        connection_healthy_ = false;
        return Report_::Disconnected;
    }
    else {
        for (int i = 0; i < response; i++) {
            if (inbuffer_[i] == '\r') {  //this symbol prints variably in terminals
                inbuffer_[i] = '|';      //this symbol should never occur in the denon protocol
            }
        }
        inchain_.append(inbuffer_.data(), response);
        Print(Verbosity::Debug, std::format("Received following from AVR: [{}]\n    Full chain now "
                                            "contains: [{}]\n", std::string_view(inbuffer_.data(), 
                                            response), inchain_));
        return Report_::Data;
    }
}

auto AVRHandler::Parse() -> Report_ {
    if (!inmessage_.empty()) {
        Print(Verbosity::Warning, "Tried to parse new message before previous was processed\n");
        return Report_::Wait;
    }
    if (inchain_.empty()) {
        Print(Verbosity::Trace, "Chain is empty, nothing to parse\n");
        return Report_::NoData;
    }
    Print(Verbosity::Debug, "Parsing received message chain\n");
    auto index = inchain_.find('|');
    if (index == std::string::npos) {
        Print(Verbosity::Debug, std::format("Chain does not contain \\r: [{}]\n", inchain_));
        return Report_::NoData;
    }
    else if (index < 1) {
        Print(Verbosity::Warning, "Chain contained \\r character at position 0\n");
        inchain_.erase(0, 1);
        return Report_::BadInput;
    }
    else {
        inmessage_ = inchain_.substr(0, index);
        inchain_.erase(0, index + 1);
        Print(Verbosity::Info, std::format("Received message: [{}]\n", inmessage_));
        Print(Verbosity::Trace, std::format("Remaining content of chain: [{}]\n", inchain_));
        return Report_::Data;
    }
}

auto AVRHandler::Send(std::string_view cmd, bool wait) -> Report_ {
    if (command_cooldown_.IsPending()) {
        if (wait) {
            Print(Verbosity::Debug, "Send called within cooldown, waiting for it to end\n");
            command_cooldown_.Wait();
        }
        else {
            Print(Verbosity::Warning, std::format("Tried to send command [{}] while in cooldown.\n",
                                                cmd));
            return Report_::Wait;
        }     
    }
    std::string full{cmd};
    full += '\r';
    Print(Verbosity::Info, std::format("Sending following command to AVR: [{}]\n", cmd));
    int response = send(socket_, full.data(), static_cast<int>(full.size()), 0);
    command_cooldown_.Set();
    if (response == SOCKET_ERROR) {
        response = WSAGetLastError();
        Print(Verbosity::Warning, std::format("Failed to send command: [{}], with error: {}\n", cmd,
                                              response));
        connection_healthy_ = false;
        return Report_::SocketError;        
    }
    return Report_::OK;
}

std::string AVRHandler::MakeCommand(std::string cmd, std::optional<int> num) {
    if (num) {      //TODO: reevaluate if any usecase other than adding num occur
        if (cmd == denon_cmd::volume_prefix) {
            cmd += DbToString(*num);
        }
        else if (cmd.starts_with(denon_cmd::chanvol_prefix)) {
            cmd += DbToString(*num, 500, 380, 620);
        }
        else {
            cmd += std::to_string(*num);
        }
    }
    return cmd;
}

std::string AVRHandler::DbToString(int db_tenths, int zero, int min, int max) {
    std::string result;
    db_tenths = static_cast<int>(std::round(db_tenths / 5.0)) * 5;
    db_tenths += zero;
    if (db_tenths <= min) {
        db_tenths = min;
    }
    else if (db_tenths >= max) {
        db_tenths = max;
    }
    if (db_tenths < 100) {
        result = "0";
    }
    result += std::to_string(db_tenths / 10);
    if ((db_tenths % 10) > 0) {
        result += '5';
    }
    return result;
}

std::optional<int> AVRHandler::StringToDb(std::string_view str, int zero) {
    std::string string{str};
    if (string.size() == 2) {
        string += '0';
    }
    try {
        return std::stoi(string) - zero;
    }
    catch (const std::invalid_argument&) {
        std::cerr << std::format("[WARNING] Invalid string characters for db conversion: [{}]\n",
                                               string);
        return std::nullopt;
    }
    catch (const std::out_of_range&) {
        std::cerr << std::format("[WARNING] String too large for db conversion: [{}]\n", string);
        return std::nullopt;
    }
}

void AVRHandler::Print(Verbosity level, std::string_view msg) const {
    if (verbosity_ < level) return;
    switch (level) {
        case Verbosity::Error:
            std::cerr << "[ERROR] " << msg;
            break;
        case Verbosity::Warning:
            std::cerr << "[WARNING] " << msg;
            break;
        case Verbosity::Info:
            std::cout << msg;
            break;
        case Verbosity::Debug:
            std::cout << "[DEBUG] " << msg;
            break;
        case Verbosity::Trace:
            std::cout << "[TRACE] " << msg;
            break;
        default:
            std::cerr << "[ERROR] Print called with invalid verbosity level\n    Message: " << msg;
            break;
    }
    return;
}

void AVRHandler::PrintStates(Verbosity level) {
    if (verbosity_ < level) return;
    int w1 = 13;
    int w2 = w1 - 3;
    std::cout <<             "States:        |   Requested   |   Commanded   |   Reported   \n";
    std::cout <<             "==============================================================\n";
    std::cout << std::format("power state:   | {: >{}} | {: >{}} | {: >{}}\n",
                             requested_.power.substr(0, w1), w1, commanded_.power.substr(0, w1), w1,
                             reported_.power.substr(0, w1), w1);
    std::cout << std::format("input/source:  | {: >{}} | {: >{}} | {: >{}}\n",
                             requested_.input.substr(0, w1), w1, commanded_.input.substr(0, w1), w1,
                             reported_.input.substr(0, w1), w1);
    std::cout << std::format("surround mode: | {: >{}} | {: >{}} | {: >{}}\n",
                             requested_.surround.substr(0, w1), w1, commanded_.surround.substr(0, w1),
                             w1, reported_.surround.substr(0, w1), w1);
    std::cout << std::format("mute:          | {: >{}} | {: >{}} | {: >{}}\n", 
                             requested_.mute.substr(0, w1), w1, commanded_.mute.substr(0, w1), w1,
                             reported_.mute.substr(0, w1), w1);
    std::cout << std::format("volume:        | {} | {} | {}\n", PrintDb(requested_.volume, w2), 
                             PrintDb(commanded_.volume, w2), PrintDb(reported_.volume, w2));
    std::cout << std::format("maxvolume:     | {} | {} | {}\n", PrintDb(requested_.maxvolume, w2), 
                             PrintDb(commanded_.maxvolume, w2), PrintDb(reported_.maxvolume, w2));
    std::cout << std::format("chanvol FL:    | {} | {} | {}\n", PrintDb(requested_.chanvol.FL, w2), 
                             PrintDb(commanded_.chanvol.FL, w2), PrintDb(reported_.chanvol.FL, w2));
    std::cout << std::format("chanvol FR:    | {} | {} | {}\n", PrintDb(requested_.chanvol.FR, w2), 
                             PrintDb(commanded_.chanvol.FR, w2), PrintDb(reported_.chanvol.FR, w2));
    std::cout << std::format("chanvol C:     | {} | {} | {}\n", PrintDb(requested_.chanvol.C, w2), 
                             PrintDb(commanded_.chanvol.C, w2), PrintDb(reported_.chanvol.C, w2));
    std::cout << std::format("chanvol SW:    | {} | {} | {}\n", PrintDb(requested_.chanvol.SW, w2),
                             PrintDb(commanded_.chanvol.SW, w2), PrintDb(reported_.chanvol.SW, w2));
    std::cout << std::format("chanvol SL:    | {} | {} | {}\n", PrintDb(requested_.chanvol.SL, w2), 
                             PrintDb(commanded_.chanvol.SL, w2), PrintDb(reported_.chanvol.SL, w2));
    std::cout << std::format("chanvol SR:    | {} | {} | {}\n", PrintDb(requested_.chanvol.SR, w2), 
                             PrintDb(commanded_.chanvol.SR, w2), PrintDb(reported_.chanvol.SR, w2));
}

std::string AVRHandler::PrintDb(int value, int width) {
    if (width == 0) {
        if (value == 0) {
            return "0 db";
        }
        else {
            return std::format("{:+.1f} db", value / 10.0);
        }
    }
    else {
        if (value == 0) {
            return std::format("{:{}.1f} db", value / 10.0, width);
        }
        else {
            return std::format("{:+{}.1f} db", value / 10.0, width);
        }
    }
}
