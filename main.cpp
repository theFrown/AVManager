#include <iostream>
#include <format>
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
    int response = log.Start(true) ? 0 : 1;
    if (response != 0) {
        std::cerr << "[ERROR] Logger failed to start with error code: " << response << "\n";
        return 1;
    }
    
    {
        auto log_lambda = [&log](std::string_view message) -> bool { return log.Log(message); };
        log.Log("AVManager starting");
        AVRHandler avr{log_lambda, Verbosity::Debug};
        AVRHandler::CtorCode avr_status = avr.GetCtorStatus();
        if (avr_status != AVRHandler::CtorCode::Healthy) {
            log.Log(std::format("[ERROR] AVRHandler creation failed with error code: {}",
                    static_cast<int>(avr_status)));
            return 2;
        }

        log.Log("KeyboardHook starting");
        KeyboardHook keyhook;
        response = keyhook.Start() ? 0 : 1;
        if (response != 0) {
            log.Log("[ERROR] KeyboardHook failed to start");
            return 3;
        }   

        log.Log("Main loop starting\n=============================================\n ");
        AVRHandler::ExitCode exitcode = avr.ControlLoop();
        log.Log("\n=============================================", false);
        log.Log("Main loop ended");
        keyhook.Stop();
        if ((exitcode != AVRHandler::ExitCode::Normal) && (exitcode != AVRHandler::ExitCode::Commanded)) {
            log.Log(std::format("[ERROR] Main loop ended uncommanded with error: {}", 
                    static_cast<int>(exitcode)));
            return 4;
        }
    }
    return 0;
}
