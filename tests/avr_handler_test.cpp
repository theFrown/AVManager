#include "avr_handler.h"
#include <utility>
#include <string>
#include <format>
#include <gtest/gtest.h>

TEST(DbToString, ClampsDefault) {
    //the three constants below reflect the *communication protocol*, not any current AVR *setting*
    const int zero = 800;
    const int min  =   0;
    const int max  = 980;

    const int step  = 5; //an AVR setting that may change `DbToString` behavior in the future
    const int lower = min - zero;
    const int upper = max - zero;
    const char* lower_string      = "00";
    const char* lowerplus_string  = "005";
    const char* upper_string      = "98";
    const char* upperminus_string = "975";

    std::pair<int, const char*> clamps[] = {
        {lower - 1523, lower_string},     //handles widely out of range
        {lower -   10, lower_string},     //handles clamp without rounding
        {lower - step, lower_string},     //current implementation solves this accidentally
        {lower -    1, lower_string},     //should be solved by either rounding *or* clamping
        {lower       , lower_string},     //if this one fails our min or zero are wrong
        //'lower + 1' is skipped because its output depends on whether or not rounding works
        {lower + step, lowerplus_string}, //doesn't overclamp
        {upper + 1523, upper_string},     //repeat all the same, but now for upper ...
        {upper + step, upper_string},
        {upper +    1, upper_string},
        {upper       , upper_string},
        //'upper - 1' is skipped for same reason
        {upper - step, upperminus_string}
    };

    for (const auto& [in, out] : clamps) {
        EXPECT_EQ(AVRHandler::DbToString(in), out) << " input: " << in;
    }
}

TEST(DbToString, ClampsChannel) {
    //the three constants below reflect the *communication protocol*, not any current AVR *setting*
    const int zero_in  = 500;
    const int min_in   = 380;
    const int max_in   = 620;

    const int step  = 5; //an AVR setting that may change `DbToString` behavior in the future
    const int lower = min_in - zero_in;
    const int upper = max_in - zero_in;
    const char* lower_string      = "38";
    const char* lowerplus_string  = "385";
    const char* upper_string      = "62";
    const char* upperminus_string = "615";

    struct channel_args {
        int value = 0;
        int zero  = zero_in;
        int min   = min_in;
        int max   = max_in;
    };

    std::pair<channel_args, const char*> clamps[] = {
        {{lower - 1523}, lower_string},     //handles widely out of range
        {{lower - step}, lower_string},     //handles clamp without rounding
        {{lower -    1}, lower_string},     //should be solved by either rounding *or* clamping
        {{lower       }, lower_string},     //if this one fails the arguments are handled wrong
        //'lower + 1' is skipped because its output depends on whether or not rounding works
        {{lower + step}, lowerplus_string}, //doesn't overclamp
        {{upper + 1523}, upper_string},     //repeat all the same, but now for upper ...
        {{upper + step}, upper_string},
        {{upper +    1}, upper_string},
        {{upper       }, upper_string},
        //'upper - 1' is skipped for same reason
        {{upper - step}, upperminus_string}
    };

    for (const auto& [in, out] : clamps) {
        EXPECT_EQ(AVRHandler::DbToString(in.value, in.zero, in.min, in.max), out) << " inputs:\n"
            << std::format("    value: {}, zero: {}, min: {}, max: {}", in.value, in.zero, in.min,
                           in.max);
    }
}
