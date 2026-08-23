#include <iostream>
#include <array>
#include <optional>
#include <stdexcept>
#include <format>
#include <chrono>
#include <thread>
#include <string>
#include <cmath>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "AVRHandler.h"
#include "DenonProtocol.h"

AVRHandler::AVRHandler(Verbosity verbosity) 
    : verbosity_(verbosity)
{   
    status = 1;
    Report_ response = SetupSocket();
    if (response != Report_::OK) {
        status = 2;
    }
    else {
        response = Connect();
        if (response != Report_::OK) {
            status = 3;
        }
        else {
            status = 0;
            stayalive = true;
            connection_healthy_ = true;
        }
    }
}

AVRHandler::~AVRHandler() {
    Print("Shutting down AVRHandler\n");
    if (socket_ != INVALID_SOCKET) {
        int response = shutdown(socket_, SD_SEND);
        if (response == 0) {
            TimePoint_ kill_deadline = Clock_::now() + MilliSeconds_(2000);
            Report_ coms_response;
            bool listening = true;
            connection_shutting_down_ = true;
            while (listening) {
                coms_response = ControlReceive(10);
                if (coms_response == Report_::Disconnected) {
                    Print("Connection shutdown confirmed");
                    listening = false;
                }
                else if (coms_response == Report_::SocketError) {
                    Print("Connection error, forcing shutdown"); 
                    listening = false;
                }
                if (Clock_::now() > kill_deadline) {
                    Print("Connection timed out, forcing shutdown");
                    listening = false;
                }
            }
        }
        else {
            Print(Verbosity::Warning, std::format("Socket shutdown failed with response:\n    {}\n"
                                                  "    proceding with hard shutdown\n", response));
            linger value{.l_onoff = 1, .l_linger = 0};
            const char* value_byteaddress = reinterpret_cast<const char*>(&value);
            setsockopt(socket_, SOL_SOCKET, SO_LINGER, value_byteaddress, sizeof(value));
        }
        closesocket(socket_);
    }
    WSACleanup();
}

int AVRHandler::ControlLoop() {
    Report_ response = ControlResync();
    if (response != Report_::OK) {
        return 1; //issue with syncing
    }
    
    auto test_deadline = Clock_::now() + MilliSeconds_(50);
    int test_stage = 0;
    // int test_db = -300;
    
    while (stayalive) {

        //test infrastructure below v v
        if (Clock_::now() > test_deadline) {
            switch (test_stage) {
                case 0:
                    Print("\n-------volume to -35db-------\n\n");
                    requested_.volume = -350;
                    control_mode_ = ControlMode_::Request;
                    test_deadline = Clock_::now() + MilliSeconds_(10000);
                    break;
                case 1:
                    Print("\n------------volume to -26db------------\n\n");
                    requested_.volume = -260;
                    control_mode_ = ControlMode_::Request;
                    test_deadline = Clock_::now() + MilliSeconds_(2500);
                    break;
                case 2:
                    Print("\n------------volume up to -30db------------\n\n");
                    requested_.volume = -300;
                    control_mode_ = ControlMode_::Request;
                    test_deadline = Clock_::now() + MilliSeconds_(2500);
                    break;
                // case 3:
                //     Print("\n------------injecting denon report to -29db------------\n\n");
                //     inchain_.append("MV51|");
                //     test_deadline = Clock_::now() + MilliSeconds_(1000);
                //     break;
                // case 4:
                //     if (test_db > -400) {
                //         Print("\n------------injecting rapid denon reports (-30db to -40db)------------\n\n");
                //         test_stage--; //keeps us in case 4
                //         test_db -= 5;
                //         inchain_.append(std::format("MV{}|",dbtostring(test_db)));
                //     }
                //     test_deadline = Clock_::now() + MilliSeconds_(10);
                //     break;
                // case 5:
                //     Print("\n------------corruption test: set volume to -31db------------\n\n");
                //     requested_.volume = -310;
                //     control_mode_ = ControlMode_::Request;
                //     test_deadline = Clock_::now() + MilliSeconds_(1000);
                //     break;
                // case 6:
                //     Print("\n------------injecting bad denon report after 1000ms------------\n\n");
                //     inchain_.append("MV41|");
                //     test_deadline = Clock_::now() + MilliSeconds_(1000);
                //     break;
                // case 7:
                //     Print("\n------------corruption test: set volume to -30db------------\n\n");
                //     requested_.volume = -300;
                //     control_mode_ = ControlMode_::Request;
                //     test_deadline = Clock_::now() + MilliSeconds_(150);
                //     break;
                // case 8:
                //     Print("\n------------injecting denon report after 150ms------------\n\n");
                //     inchain_.append("MV42|");
                //     test_deadline = Clock_::now() + MilliSeconds_(1000);
                //     break;
                // case 9:
                //     Print("\n------------corruption test: set volume to -31db------------\n\n");
                //     requested_.volume = -310;
                //     control_mode_ = ControlMode_::Request;
                //     test_deadline = Clock_::now() + MilliSeconds_(10);
                //     break;
                // case 10:
                //     Print("\n------------injecting denon report after 10ms------------\n\n");
                //     inchain_.append("MV41|");
                //     test_deadline = Clock_::now() + MilliSeconds_(2000);
                //     break;
                default:
                    stayalive = false;
                    break;
            }
            test_stage++;
        }
        //test infrastructure above ^ ^

        //receiving: making sure reported_ is up-to-date through SyncIn
        response = ControlReceive(10);
        if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
            return 2; //issue in receive loop
        }

        //sending: making sure commanded_ is up-to-date through SyncOut
        if ((commanded_ != requested_) && (Clock_::now() > command_cooldown_)) {
            response = SyncOut();
            if (response != Report_::OK) {
                return 3; //problem with Send
            }
        }

        //closing the loop: making sure commanded_ and reported_ reconcile
        if (control_mode_ != ControlMode_::Rest) { 
            response = SyncResolve();
            if (response == Report_::Unknown) {
                response = ControlResync();
                if (response != Report_::OK) {
                    return 1; //issue with syncing
                }
            }
        }
    }
    return 0;
}

auto AVRHandler::ControlPoll(bool block, int time_out) -> Report_ {
    Print(Verbosity::Debug, "Sending test command to AVR\n");
    Report_ response = Send(denon_cmd::volume_status, block);
    signal_ = denon_cmd::volume_maxprefix;
    signal_received_ = false;
    bool succeeded = false;
    if ((!block) || (response != Report_::OK)) {
        return response;
    }
    else {
        if (time_out > 0) {
            Print(Verbosity::Debug, std::format("Waiting until we receive expected respone (up to {} ms)", time_out));
            auto deadline = Clock_::now() + MilliSeconds_(time_out);
        }
        else {
            Print(Verbosity::Debug, "Waiting until we receive expected respone");
        }
        while (!signal_received_) {
            response = ControlReceive(10);
            if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
                return response;
            }
            if (signal_received_) {
                Print(Verbosity::Trace, "Parameter successfully synced\n");
                succeeded = true;
                break;
            }
            else if (Clock_::now() > deadline) {
                Print(Verbosity::Warning, std::format("Sync stage {} timed out\n", stage)); 
                sent = false;
                attempt++;
            }
            if ((time_out > 0)
        }
    }
}


auto AVRHandler::ControlResync() -> Report_ {
    Print("Syncing all parameters from the AVR\n");
    PrintStates(Verbosity::Debug);
    TimePoint_ deadline;
    Report_ response = Report_::Unknown;
    int attempt;
    bool sent;
    bool succeeded;
    //TODO: might be smart to sleep for 'patience' ms (or more) to purge denon incoming messages 
    for (int stage = 0; stage < 4; stage++) {
        attempt = 1;
        sent = false;
        succeeded = false;
        while (attempt < 4) {
            if (!sent) {
                switch (stage) {
                    case 0:
                        Print(Verbosity::Debug, "Syncing input/source\n");
                        response = Send(denon_cmd::input_status, true);
                        signal_ = denon_cmd::input_prefix; //read as 'last message we care about'
                        break;                             //more may follow and are ignored
                    case 1:
                        Print(Verbosity::Debug, "Syncing surround mode\n");
                        response = Send(denon_cmd::surround_status, true);
                        signal_ = denon_cmd::surround_prefix;
                        break;
                    case 2:
                        Print(Verbosity::Debug, "Syncing master volume\n");
                        response = Send(denon_cmd::volume_status, true);
                        signal_ = denon_cmd::volume_maxprefix;
                        break;
                    case 3:
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
                    deadline = Clock_::now() + MilliSeconds_(1000);
                }
                else {
                    return response;
                }
            }
            else {
                response = ControlReceive(10);
                if ((response == Report_::SocketError) || (response == Report_::Disconnected)) {
                    return response;
                }
                if (signal_received_) {
                    Print(Verbosity::Trace, "Parameter successfully synced\n");
                    succeeded = true;
                    break;
                }
                else if (Clock_::now() > deadline) {
                    Print(Verbosity::Warning, std::format("Sync stage {} timed out\n", stage)); 
                    sent = false;
                    attempt++;
                }
            }
        }
        if (!succeeded) {
            Print(Verbosity::Warning, std::format("Resync failed, exceeded retry attempts for stage {}\n", stage));
            return Report_::Unknown; //TODO: see if it's worth adding a 'timeout' return value
        }
    }
    Print(Verbosity::Info, "Full resync succesful, state parameters now:\n");
    PrintStates(Verbosity::Info);
    return Report_::OK;
}

auto AVRHandler::ControlReceive(int time_out) -> Report_ {
    Report_ response = CheckIncoming(time_out);
    if (response == Report_::SocketError) return response;
    if (response == Report_::Data) {
        response = Receive();
        if ((response == Report_::SocketError) || (response == Report_::Disconnected)) return response;
    }
    if (inchain_.empty()) return response; //draining inchain_ is more important than transparency
    while ((response = Parse()) != Report_::NoData) { //effectively: if there's something in inchain_ worth parsing
        if (!inmessage_.empty()) {
          response = SyncIn();
          if (inchain_.empty() && response != Report_::OK) return response;
        }
    }
    return Report_::OK;
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
    if (message.starts_with(denon_cmd::input_prefix)) {
        return SyncInString(message, denon_cmd::input_prefix, "input source", reported_.input);
    }
    else if (message.starts_with(denon_cmd::surround_prefix)) {
        return SyncInString(message, denon_cmd::surround_prefix, "surround mode", reported_.surround);
    }
    else if (message.starts_with(denon_cmd::volume_prefix)) {
        if (message.starts_with(denon_cmd::volume_maxprefix)) {
            return SyncInDb(message, denon_cmd::volume_maxprefix, "max volume", 
                                 reported_.maxvolume);
        }
        else {
            return SyncInDb(message, denon_cmd::volume_prefix, "volume", reported_.volume);
        }
    }
    else if (message.starts_with(denon_cmd::chanvol_prefix)) {
        if (message.starts_with(denon_cmd::chanvol_endreport)) {
            Print(Verbosity::Debug, "Channel volume list complete\n");
            return Report_::OK;
        }
        else if (message.starts_with(denon_cmd::chanvol_FL_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_FL_prefix, "FL volume", 
                    reported_.chanvol.FL));
        }
        else if (message.starts_with(denon_cmd::chanvol_FR_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_FR_prefix, "FR volume", 
                    reported_.chanvol.FR));
        }
        else if (message.starts_with(denon_cmd::chanvol_C_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_C_prefix, "C volume", 
                    reported_.chanvol.C));
        }
        else if (message.starts_with(denon_cmd::chanvol_SW_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_SW_prefix, "SW volume", 
                    reported_.chanvol.SW));
        }
        else if (message.starts_with(denon_cmd::chanvol_SL_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_SL_prefix, "SL volume", 
                    reported_.chanvol.SL));
        }
        else if (message.starts_with(denon_cmd::chanvol_SR_prefix)) {
            return (SyncInDb(message, denon_cmd::chanvol_SR_prefix, "SR volume", 
                    reported_.chanvol.SR));
        }
        else {
            auto p = message.substr(std::string_view(denon_cmd::chanvol_prefix).size());
            Print(Verbosity::Warning, std::format("Unknown parameter [{}] for channel volume\n", p));
            return Report_::Unknown;
        }
    }
    else {
        Print(Verbosity::Debug, std::format("Unrecognised message [{}]\n", message));
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
    auto num = stringtodb(params);
    if (!num) {
        Print(Verbosity::Warning, std::format("Parsing incoming message [{}] failed.\n    params [{}]"
                                              " do not convert cleanly to an int\n", prefix, params));
        return Report_::BadInput;
    }
    Print(Verbosity::Debug, std::format("Denon reports {} {:+} db\n", report_string, *num / 10.0));
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
    Report_ response = Report_::Unknown;
    if (commanded_.volume != requested_.volume) {
        control_mode_ = ControlMode_::Request;
        response = Send(MakeCommand(denon_cmd::volume_prefix, requested_.volume));
        if (response == Report_::OK) {
            commanded_.volume = requested_.volume;
        }
    }
    else if (commanded_.maxvolume != requested_.maxvolume) {
        control_mode_ = ControlMode_::Request;
        response = Send(MakeCommand(denon_cmd::volume_maxprefix, requested_.maxvolume));
        if (response == Report_::OK) {
            commanded_.maxvolume = requested_.maxvolume;
        }
    }
    PrintStates(Verbosity::Debug);
    return response;
}

auto AVRHandler::SyncResolve() -> Report_ {
    if (control_mode_ == ControlMode_::Request) {
        if (Clock_::now() < response_deadline_) { 
            return Report_::Wait;
        }
        else {
            if ((reported_ == commanded_ ) && (commanded_ == requested_)) {
                Print(Verbosity::Info, "All Commands succesfully sent and confirmed\n");
                failed_syncs_ = 0;
                control_mode_ = ControlMode_::Rest;
                return Report_::OK;
            }
            else {
                failed_syncs_++;
                commanded_ = reported_;  //forces desync between requested_ and commanded_
                if (failed_syncs_ > 5) {
                    Print(Verbosity::Warning, "Sync attempts keep failing, forcing a full Resync from AVR\n"
                                              "    Resetting unconfirmed commands as unsent.\n");
                    failed_syncs_ = 0;
                    return Report_::Unknown;
                }
                else {
                    Print(Verbosity::Warning, "One or more commands have not been correctly reported back\n"
                                              "    Resetting unconfirmed commands as unsent.\n");
                    PrintStates(Verbosity::Debug);
                    return Report_::Data;
                }
            }
        }
    }
    else {
        if (control_mode_ == ControlMode_::Report) {
            Print(Verbosity::Info, "State succesfully updated by AVR-originated change\n");
        }
        else {
            Print(Verbosity::Warning, "SyncResolve called for unknown reasons,\n"
                                      "    harmonizig all states to reported state\n");
        }
        commanded_ = requested_ = reported_;
        failed_syncs_ = 0;
        response_deadline_ = Clock_::now();
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
        Print(Verbosity::Error, std::format("WSAStartup failed with response:\n    {}\n", response));
        return Report_::SocketError;
    }
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        Print(Verbosity::Error, "Failed to create socket\n");
        return Report_::SocketError;
    }
    sockaddr_.sin_family = AF_INET;
    sockaddr_.sin_port   = htons(port_);
    response = inet_pton(AF_INET, ip_string_, &sockaddr_.sin_addr);
    if (response != 1) {
        Print(Verbosity::Error, "Failed to convert IP address\n");
        return Report_::BadInput;
    }
    return Report_::OK;
}

auto AVRHandler::Connect() -> Report_ {
    Print(Verbosity::Info, "Opening connection\n");
    connection_healthy_ = false;
    int response = connect(socket_, reinterpret_cast<const sockaddr*>(&sockaddr_), sizeof(sockaddr_));
    if (response != 0) { 
        Print(Verbosity::Error, std::format("Failed to connect socket with response:\n    {}\n", 
                                            response));
        return Report_::SocketError;
    }
    connection_healthy_ = true; //TODO, consider waiting until confirmed with handshake
    return Report_::OK;
}

auto AVRHandler::CheckIncoming(int time_out) -> Report_ {
    Print(Verbosity::Debug, "Checking for incoming messages\n");
    timeval tv{0, time_out * 1000};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(socket_, &readfds);
    int response = select(0, &readfds, nullptr, nullptr, &tv);
    if (response == 0) {
        Print(Verbosity::Trace, "No incoming messages\n");
        return Report_::NoData;
    }
    else if (response == SOCKET_ERROR) {
        Print(Verbosity::Warning, "Issue with checking for messages\n");
        connection_healthy_ = false;
        return Report_::SocketError;
    }
    else if (FD_ISSET(socket_, &readfds)) {
        Print(Verbosity::Debug, "Incoming message from AVR\n");
        return Report_::Data;
    }
    else {
        Print(Verbosity::Warning, "Incoming message from a mystery socket\n");
        return Report_::Unknown;
    }
}

auto AVRHandler::Receive() -> Report_ {
    Print(Verbosity::Trace, "Receiving incoming message\n");
    int response = recv(socket_, inbuffer_.data(), static_cast<int>(inbuffer_.size()), 0);
    if (response == SOCKET_ERROR) { 
        Print(Verbosity::Warning, "Failed to retreive response from AVR\n");
        connection_healthy_ = false;
        return Report_::SocketError;        
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
        Print(Verbosity::Warning, "Tried to parse new message before previous was processed");
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
    if (Clock_::now() < command_cooldown_) {
        if (wait) { 
            Print(Verbosity::Debug, "Send called within cooldown, waiting for it to end\n");
            std::this_thread::sleep_until(command_cooldown_);
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
    command_cooldown_ = Clock_::now() + cooldown_default_;
    response_deadline_ = Clock_::now() + patience_default_;
    if (response == SOCKET_ERROR) {
        Print(Verbosity::Warning, std::format("Failed to send command: [{}]\n", cmd));
        connection_healthy_ = false;
        return Report_::SocketError;        
    }
    return Report_::OK;
}

std::string AVRHandler::MakeCommand(std::string cmd, std::optional<int> num) const {
    if (num) {      //TODO: reevaluate if any usecase other than adding num occur
        if (cmd.compare(denon_cmd::volume_prefix) == 0) {
            cmd += dbtostring(*num);
        }
        else {
            cmd += std::to_string(*num);
        }
    }
    return cmd;
}

std::string AVRHandler::dbtostring(int db) const {
    std::string result;
    db = static_cast<int>(std::round(db / 5.0)) * 5;
    db += 800;
    if (db <= 0) {
        result = "00";
    }
    else if (db >= 980) {
        result = "98";
    }
    else {
        if (db < 100) {
            result = "0";
        }
        result += std::to_string(db / 10);
        if ((db % 10) > 0) {
            result += '5';
        }
    }
    return result;
}

std::optional<int> AVRHandler::stringtodb(std::string_view str) const {
    std::string string{str};
    if (string.size() == 2) {
        string += '0';
    }
    try {
        return std::stoi(string) - 800;
    }
    catch (const std::invalid_argument&) {
        Print(Verbosity::Warning,  std::format("Invalid string characters for db conversion: [{}]\n",
                                               string));
        return std::nullopt;
    }
    catch (const std::out_of_range&) {
        Print(Verbosity::Warning, std::format("String too large for db conversion: [{}]\n", string));
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
    std::cout <<             "States:         |   Requested   |   Commanded   |   Reported   \n";
    std::cout <<             "===============================================================\n";
    std::cout << std::format("input/source  : | {: >{}} | {: >{}} | {: >{}}\n",
                             requested_.input.substr(0, w1), w1, commanded_.input.substr(0, w1), w1,
                             reported_.input.substr(0, w1), w1);
    std::cout << std::format("surround mode : | {: >{}} | {: >{}} | {: >{}}\n",
                             requested_.surround.substr(0, w1), w1, commanded_.surround.substr(0, w1), 
                             w1, reported_.surround.substr(0, w1), w1);
    std::cout << std::format("volume:         | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.volume / 10.0, w2, commanded_.volume / 10.0, w2, 
                             reported_.volume / 10.0, w2);
    std::cout << std::format("maxvolume:      | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.maxvolume / 10.0, w2, commanded_.maxvolume / 10.0, w2, 
                             reported_.maxvolume / 10.0, w2);
    std::cout << std::format("chanvol FL:     | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.FL / 10.0, w2, commanded_.chanvol.FL / 10.0, w2, 
                             reported_.chanvol.FL / 10.0, w2);
    std::cout << std::format("chanvol FR:     | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.FR / 10.0, w2, commanded_.chanvol.FR / 10.0, w2, 
                             reported_.chanvol.FR / 10.0, w2);
    std::cout << std::format("chanvol C:      | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.C / 10.0, w2, commanded_.chanvol.C / 10.0, w2, 
                             reported_.chanvol.C / 10.0, w2);
    std::cout << std::format("chanvol SW:     | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.SW / 10.0, w2, commanded_.chanvol.SW / 10.0, w2, 
                             reported_.chanvol.SW / 10.0, w2); 
    std::cout << std::format("chanvol SL:     | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.SL / 10.0, w2, commanded_.chanvol.SL / 10.0, w2, 
                             reported_.chanvol.SL / 10.0, w2);
    std::cout << std::format("chanvol SR:     | {:+{}.1f} db | {:+{}.1f} db | {:+{}.1f} db\n", 
                             requested_.chanvol.SR / 10.0, w2, commanded_.chanvol.SR / 10.0, w2, 
                             reported_.chanvol.SR / 10.0, w2);
}