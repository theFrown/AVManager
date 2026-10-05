#include <iostream>
#include "AVRHandler.h"
#include "keyboard.h"
#include "timing.h"

int main() {

    #ifndef NDEBUG
    std::cout << std::unitbuf;
    #endif

    std::cout << "AVManager starting\n";
    AVRHandler avr{Verbosity::Info};
    AVRHandler::CtorCode avr_status = avr.GetCtorStatus();
    if (avr_status != AVRHandler::CtorCode::Healthy) {
        std::cout << "[ERROR] AVRHandler creation failed with error code: ";
        std::cout << static_cast<int>(avr_status) << "\n";
        return 1;
    }

    std::cout << "KeyboardHook starting\n";
    KeyboardHook keyhook;
    int response = keyhook.Start() ? 0 : 1;
    if (response != 0) {
        std::cout << "KeyboardHook failed to start\n";
        return 2;
    }   

    std::cout << "Main loop starting\n=============================================\n\n";
    AVRHandler::ExitCode exitcode = avr.ControlLoop();
    std::cout << "\n=============================================\nMain loop ended\n";
    keyhook.Stop();
    if ((exitcode != AVRHandler::ExitCode::Normal) && (exitcode != AVRHandler::ExitCode::Commanded)) {
        std::cout << "[ERROR] Main loop ended uncommanded with error:\n";
        std::cout << static_cast<int>(exitcode) << "\n";
        return 3;
    }   
    return 0;
}
