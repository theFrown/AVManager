#pragma once

namespace denon_cmd {
    constexpr const char* volumeprefix    = "MV";
    constexpr const char* volumemaxprefix = "MVMAX ";
    constexpr const char* volumestatus    = "MV?\r";
    constexpr const char* volumeup        = "MVUP\r";
    constexpr const char* volumedown      = "MVDOWN\r";
    constexpr const char* inputprefix = "SI";
    constexpr const char* inputstatus = "SI?\r";
    constexpr const char* inputTV     = "SITV\r";
    constexpr const char* surroundprefix = "MS";
    constexpr const char* surroundstatus = "MS?\r";
    constexpr const char* surroundmatrix = "MSMATRIX\r";
}