#include <iostream>
#include "AVRHandler.h"
#include "keyboard.h"
#include "timing.h"
#include "logger.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    std::cout << "Logger starting\n";
    Logger log;
    int response = log.Start(true);
    if (response != 0) {
        std::cerr << "[ERROR] Logger failed to start with error code: " << response << "\n";
        return 1;
    }
    
    std::cout << "AVManager starting\n";
    AVRHandler avr{Verbosity::Info};
    AVRHandler::CtorCode avr_status = avr.GetCtorStatus();
    if (avr_status != AVRHandler::CtorCode::Healthy) {
        std::cerr << "[ERROR] AVRHandler creation failed with error code: ";
        std::cerr << static_cast<int>(avr_status) << "\n";
        return 2;
    }

    std::cout << "KeyboardHook starting\n";
    KeyboardHook keyhook;
    response = keyhook.Start() ? 0 : 1;
    if (response != 0) {
        std::cerr << "[ERROR] KeyboardHook failed to start\n";
        return 3;
    }   

    std::cout << "Main loop starting\n=============================================\n\n";
    AVRHandler::ExitCode exitcode = avr.ControlLoop();
    std::cout << "\n=============================================\nMain loop ended\n";
    keyhook.Stop();
    if ((exitcode != AVRHandler::ExitCode::Normal) && (exitcode != AVRHandler::ExitCode::Commanded)) {
        std::cerr << "[ERROR] Main loop ended uncommanded with error:\n";
        std::cerr << static_cast<int>(exitcode) << "\n";
        return 4;
    }
    return 0;
}
