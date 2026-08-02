#include <iostream>
#include <array>
#include <optional>
#include <stdexcept>
#include <format>
#include <chrono>
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
                coms_response = ControlReceive();
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
            BOOL value = TRUE;
            const char* value_byteaddress = reinterpret_cast<const char*>(&value);
            setsockopt(socket_, SOL_SOCKET, SO_DONTLINGER, value_byteaddress, sizeof(value));
        }
        closesocket(socket_);
    }
    WSACleanup();
}

int AVRHandler::ControlLoop() {
    auto test_deadline = Clock_::now() + MilliSeconds_(2000);
    int test_stage = 0;
    // int test_db = -300;
    Report_ response{Report_::Unknown};
    while (stayalive) {

        //test infrastructure below v v
        if (Clock_::now() > test_deadline) {
            switch (test_stage) {
                case 0:
                    Print("\n------------volume up to -28db------------\n\n");
                    requested_.volume = -280;
                    in_charge_ = InCharge_::Request;
                    test_deadline = Clock_::now() + MilliSeconds_(2500);
                    break;
                case 1:
                    Print("\n------------volume down to -32db------------\n\n");
                    requested_.volume = -320;
                    in_charge_ = InCharge_::Request;
                    test_deadline = Clock_::now() + MilliSeconds_(2500);
                    break;
                case 2:
                    Print("\n------------volume up to -30db------------\n\n");
                    requested_.volume = -300;
                    in_charge_ = InCharge_::Request;
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
                //     in_charge_ = InCharge_::Request;
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
                //     in_charge_ = InCharge_::Request;
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
                //     in_charge_ = InCharge_::Request;
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
        response = ControlReceive();
        if (response != Report_::NoData) {
            return 1; //issue in receive loop
        }

        //sending: making sure commanded_ is up-to-date through SyncOut
        if ((commanded_ != requested_) && (Clock_::now() > command_cooldown_)) {
            response = SyncOut();
            if (response != Report_::OK) {
                return 2; //problem with Send
            }
        }

        //closing the loop: making sure commanded_ and reported_ reconcile
        if (!fully_synced_) { 
            response = SyncResolve();
        }
    }
    return 0;
}

int AVRHandler::testcoms() {
    Verbosity verbosity_backup = verbosity_;
    verbosity_ = Verbosity::Trace;
    Print(Verbosity::Debug, "Sending test command to AVR\n");
    PrintStates(Verbosity::Debug);
    //int response = Send(MakeCommand(denon_cmd::volumeprefix, -200));
    Report_ response = Send(denon_cmd::volumestatus);
    if (response != Report_::OK) {
        verbosity_ = verbosity_backup;
        return 1;
    }
    for (int i = 0; i < 20; i++) {
        if (CheckIncoming(10) == Report_::Data) {
            response = Receive();
            if (response != Report_::Data) {
                verbosity_ = verbosity_backup;
                return 2;
            }
            while (inchain_.size() > 0) {
                response = Parse();
                if (response != Report_::Data) {
                    verbosity_ = verbosity_backup;
                    return 3;
                }
                response = SyncIn();
                if (response != Report_::OK) {
                    verbosity_ = verbosity_backup;
                    return 4;
                }
            }
        }
    }
    PrintStates(Verbosity::Debug);
    verbosity_ = verbosity_backup;
    return 0;
}

auto AVRHandler::ControlReceive() -> Report_ {
    Report_ response = CheckIncoming(10);
    if (response != Report_::Data) return response;
    response = Receive();
    if (response != Report_::Data) return response;
    while ((response = Parse()) == Report_::Data) {
        response = SyncIn();
        if (response != Report_::OK) return response;
    }
    return response; //NB: will always return response from Parse
}

auto AVRHandler::SyncIn() -> Report_ {
    if (inmessage_.empty()) {
        Print(Verbosity::Warning, "SyncIn called but no pending message!\n");
        return Report_::NoData;
    }
    std::string message = std::move(inmessage_);
    inmessage_.clear();  //technically probably redundant, but helps me sleep easier
    if (message.starts_with(denon_cmd::volumeprefix)) {
        if (message.starts_with(denon_cmd::volumemaxprefix)) {
            std::string_view prefix = denon_cmd::volumemaxprefix;
            std::string params = message.substr(prefix.size());
            auto num = stringtodb(params);
            if (!num) {
                Print(Verbosity::Warning, std::format("Parsing incoming message [{}] failed.\n    pa"
                                                      "rams [{}] do not convert cleanly to an int\n",
                                                      prefix, params));
                return Report_::BadInput;
            }
            Print(Verbosity::Debug, std::format("Denon reports max volume {:+} db\n", *num / 10.0));
            if (*num != reported_.maxvolume){
                reported_.maxvolume = *num;
                fully_synced_ = false;
                PrintStates(Verbosity::Debug);
            }
            else {
                Print(Verbosity::Debug, "Reported value matches internal state, message ignored\n");
            }
            return Report_::OK;
        }
        std::string_view prefix = denon_cmd::volumeprefix;
        std::string params = message.substr(prefix.size());
        auto num = stringtodb(params);
        if (!num) {
            Print(Verbosity::Warning, std::format("Parsing incoming message [{}] failed\n    "
                                                  "params [{}] do not convert cleanly to an int\n", 
                                                  prefix, params));
            return Report_::BadInput;
        }
        Print(Verbosity::Debug, std::format("Denon reports current volume {:+} db\n", *num / 10.0));
        if (*num != reported_.volume) {
            reported_.volume = *num;
            fully_synced_ = false;
            PrintStates(Verbosity::Debug);
        }
        else {
            Print(Verbosity::Debug, "Reported value matches internal state, message ignored\n");
        }
        return Report_::OK;
    }
    else {
        Print(Verbosity::Warning, std::format("SyncIn: unrecognised message [{}]\n", message));
        return Report_::Unknown;
    }
}

auto AVRHandler::SyncOut() -> Report_ {
    Report_ response{Report_::Unknown};
    if (commanded_.volume != requested_.volume) {
        fully_synced_ = false;
        in_charge_ = InCharge_::Request;
        response = Send(MakeCommand(denon_cmd::volumeprefix, requested_.volume));
        if (response == Report_::OK) {
            commanded_.volume = requested_.volume;
        }
    }
    else if (commanded_.maxvolume != requested_.maxvolume) {
        fully_synced_ = false;
        in_charge_ = InCharge_::Request;
        response = Send(MakeCommand(denon_cmd::volumemaxprefix, requested_.maxvolume));
        if (response == Report_::OK) {
            commanded_.maxvolume = requested_.maxvolume;
        }
    }
    PrintStates(Verbosity::Debug);
    return response;
}

auto AVRHandler::SyncResolve() -> Report_ {
    if (in_charge_ == InCharge_::Request) {
        if (Clock_::now() < response_deadline_) { 
            return Report_::Wait;
        }
        else {
            if ((reported_ == commanded_ ) && (commanded_ == requested_)) {
                Print(Verbosity::Info, "All Commands succesfully sent and confirmed\n");
                fully_synced_ = true;
                in_charge_ = InCharge_::Report;
                return Report_::OK;
            }
            else {
                Print(Verbosity::Warning, "One or more commands have not been correctly reported back\n    "
                    "    Ignoring unconfirmed commands and reverting to Requested state.\n");
                commanded_ = reported_;  //forces desync between requested_ and commanded_
                PrintStates(Verbosity::Debug);
                return Report_::Data;
            }
        }
    }
    else {
        Print(Verbosity::Info, "State succesfully updated by AVR-originated change\n");
        commanded_ = requested_ = reported_;
        fully_synced_ = true;
        PrintStates(Verbosity::Debug);
        response_deadline_ = Clock_::now();
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

auto AVRHandler::Send(std::string_view cmd) -> Report_ {
    if (Clock_::now() < command_cooldown_) {
        Print(Verbosity::Warning, std::format("Tried to send command [{}] while in cooldown.\n",
                                              cmd));
        return Report_::Wait;       
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
        if (cmd.compare(denon_cmd::volumeprefix) == 0) {
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
    db = static_cast<int>(round(db / 5.0)) * 5;
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

std::optional<int> AVRHandler::stringtodb(std::string str) const {
    if (str.size() == 2) {
        str += '0';
    }
    try {
        return std::stoi(str) - 800;
    }
    catch (const std::invalid_argument&) {
        Print(Verbosity::Warning,  std::format("Invalid string characters for db conversion: [{}]\n",
                                               str));
        return std::nullopt;
    }
    catch (const std::out_of_range&) {
        Print(Verbosity::Warning, std::format("String too large for db conversion: [{}]\n", str));
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
    std::cout <<             "States:    | Requested | Commanded | Reported   \n";
    std::cout <<             "===============================================\n";
    std::cout << std::format("volume:    | {:+6.1f} db | {:+6.1f} db | {:+6.1f} db\n", 
                             requested_.volume / 10.0, commanded_.volume / 10.0, 
                             reported_.volume / 10.0);
    std::cout << std::format("maxvolume: | {:+6.1f} db | {:+6.1f} db | {:+6.1f} db\n", 
                             requested_.maxvolume / 10.0, commanded_.maxvolume / 10.0, 
                             reported_.maxvolume / 10.0);
}