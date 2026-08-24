#pragma once

namespace denon_cmd {
    constexpr const char* power_prefix  = "PW";
    constexpr const char* power_status  = "PW?";
    constexpr const char* power_on      = "PWON";
    constexpr const char* power_standby = "PWSTANDBY";

    constexpr const char* volume_prefix    = "MV";
    constexpr const char* volume_status    = "MV?";
    constexpr const char* volume_up        = "MVUP";
    constexpr const char* volume_down      = "MVDOWN";
    constexpr const char* volume_maxprefix = "MVMAX ";
    
    constexpr const char* chanvol_prefix    = "CV";
    constexpr const char* chanvol_status    = "CV?";
    constexpr const char* chanvol_endreport = "CVEND";
    constexpr const char* chanvol_FL_prefix = "CVFL ";
    constexpr const char* chanvol_FR_prefix = "CVFR ";
    constexpr const char* chanvol_C_prefix  = "CVC ";
    constexpr const char* chanvol_SL_prefix = "CVSL ";
    constexpr const char* chanvol_SR_prefix = "CVSR ";
    constexpr const char* chanvol_SW_prefix = "CVSW ";

    constexpr const char* input_prefix = "SI";
    constexpr const char* input_status = "SI?";
    constexpr const char* input_TV     = "SITV";
    
    constexpr const char* surround_prefix = "MS";
    constexpr const char* surround_status = "MS?";
    constexpr const char* surround_matrix = "MSMATRIX";

    /*
    example response to "PW?|":
    PWON|

    example responses to "SI?|":
    SITV|
    SVOFF|
    SVSAT/CBL|
    VSSCHOFF|
    VSVPMAUTO|
    OPALSSET ON|
    OPALSDSP ON|
    OPALSVAL 000|
    SYSDA PCM                  |
    
    example responses to "MS?|":
    MS MULTI CH IN|
    PSDRC OFF|
    PSLFE 00|
    PSBAS 50|
    PSTRE 50|
    PSTONE CTRL OFF|
    SYSMI Multi Ch In|
    SSINFAISSIG 02|
    OPINFINS 22222222111111000000|
    SYSDA PCM                  |
    OPINFASP 22222200000000000000000000000000|

    example responses to "MV?|":
    MV50|
    MVMAX 85|

    example responses to "CV?|":
    CVFL 50|
    CVFR 50|
    CVC 54|
    CVSW 50|
    CVSL 50|
    CVSR 50|
    CVEND|
    DCAUTO|
    */
}