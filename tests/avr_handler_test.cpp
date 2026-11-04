#include "avr_handler.h"
#include <utility>
#include <format>
#include <gtest/gtest.h>

//DbToString tests are written in the order that ensures a feature that is required in subsequent
//tests is tested on its own first, e.g. formatting is tested before rounding, because rounding
//can only be tested if formatting works correctly. Formatting on the other hand avoids test cases
//that rely on rounding, shifting or clamping, because those will be tested later.

TEST(DbToString, Formats) {
    //test assumes step of 5, i.e. 0.5 dB, is valid
    const int zero =   0; //so it doesn't factor into the test, i.e. `input = expected output`
    const int min  =   0;
    const int max  = 980;

    std::pair<int, const char*> cases[] = {
        {  0, "00"},
        {  5, "005"},
        { 10, "01"},
        { 15, "015"},
        { 90, "09"},
        { 95, "095"},
        {100, "10"},
        {105, "105"},
        {110, "11"},
        {590, "59"}, //testing around a second (random) multiple of 100
        {595, "595"},
        {600, "60"},
        {605, "605"},
        {610, "61"},
        {970, "97"},
        {975, "975"},
        {980, "98"}
    };

    for (const auto& [in, out] : cases) {
        EXPECT_EQ(AVRHandler::DbToString(in, zero, min, max), out) << " inputs:\n"
            << std::format("    value: {}, zero: {}, min: {}, max: {}", in, zero, min, max);
    }
}

TEST(DbToString, ZeroShifts) {
    //test assumes step of 5, i.e. 0.5 dB, is valid
    const int min  =   0;
    const int max  = 980;

    struct args {
        int value = 0;
        int zero  = 0;
    };

    std::pair<args, const char*> cases[] = {
        {{   0,   0}, "00"},
        {{   0,   5}, "005"},
        {{   5,   0}, "005"},
        {{   5,   5}, "01"},
        {{  -5,   5}, "00"},
        {{   0,  10}, "01"},
        {{   0, 100}, "10"},
        {{ 100,   0}, "10"},
        {{ 100, 100}, "20"},
        {{-100, 300}, "20"},
        {{-200, 300}, "10"},
        {{-300, 300}, "00"},
        {{   0, 980}, "98"},
        {{ 435, 545}, "98"},
        {{ 980,   0}, "98"}
    };

    for (const auto& [in, out] : cases) {
        EXPECT_EQ(AVRHandler::DbToString(in.value, in.zero, min, max), out) << " inputs:\n"
            << std::format("    value: {}, zero: {}, min: {}, max: {}", in.value, in.zero, min, max);
    }
}

TEST(DbToString, Rounds) {
    //test assumes step of 5, i.e. 0.5 dB
    std::pair<int, int> minmaxs[] = {
        {400, 600}, //well clear of test value range
        {490, 600}, //min touching value range
        {400, 510}, //max touching value range
    };

    struct args {
        int value = 0;
        int zero  = 500; //middle of both ranges, as far away from clamping as we can get
    };

    std::pair<args, const char*> cases[] = {
        {{-10}, "49"}, //equal to min with second minmax
        {{ -9}, "49"},
        {{ -8}, "49"},
        {{ -7}, "495"},
        {{ -6}, "495"},
        {{ -5}, "495"},
        {{ -4}, "495"},
        {{ -3}, "495"},
        {{ -2}, "50"},
        {{ -1}, "50"},
        {{  0}, "50"},
        {{  1}, "50"},
        {{  2}, "50"},
        {{  3}, "505"},
        {{  4}, "505"},
        {{  5}, "505"},
        {{  6}, "505"},
        {{  7}, "505"},
        {{  8}, "51"},
        {{  9}, "51"},
        {{ 10}, "51"}, //equal to max with third minmax
        {{2, 500}, "50"}, //following 5 rows keep the sum `value + zero` constant while varying..
        {{3, 499}, "50"}, //..the split, and each checks that rounding down is unaffected.
        {{4, 498}, "50"},
        {{5, 497}, "50"},
        {{6, 496}, "50"},
        {{2, 501}, "505"}, //same routine, but now in the round-up regime
        {{3, 500}, "505"},
        {{4, 499}, "505"},
        {{5, 498}, "505"},
        {{6, 497}, "505"},
    };

    for (const auto& [min, max] : minmaxs) {
        for (const auto& [in, out] : cases) {
            EXPECT_EQ(AVRHandler::DbToString(in.value, in.zero, min, max), out) << " inputs:\n"
                << std::format("    value: {}, zero: {}, min: {}, max: {}", in.value, in.zero, min,
                               max);
        }
    }
}

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

    std::pair<int, const char*> cases[] = {
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

    for (const auto& [in, out] : cases) {
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

    struct args {
        int value = 0;
        int zero  = zero_in;
        int min   = min_in;
        int max   = max_in;
    };

    std::pair<args, const char*> cases[] = {
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

    for (const auto& [in, out] : cases) {
        EXPECT_EQ(AVRHandler::DbToString(in.value, in.zero, in.min, in.max), out) << " inputs:\n"
            << std::format("    value: {}, zero: {}, min: {}, max: {}", in.value, in.zero, in.min,
                           in.max);
    }
}

TEST(DbToString, ClampsCustom) {
    struct args {
        int value = 0;
        int zero  = 0;
        int min   = 0;
        int max   = 0;
    };

    std::pair<args, const char*> cases[] = {
        {{     0,   0,   0, 100}, "00"},  //zero == min != max
        {{     0, 100,   0, 100}, "10"},  //zero == max != min
        {{     0, 100, 100, 100}, "10"},  //zero == min == max, value 0
        {{   100, 100, 100, 100}, "10"},  //zero == min == max, positive value should be irrelevant
        {{  -100, 100, 100, 100}, "10"},  //zero == min == max, negative value should be irrelevant
        {{ -1000, 435, 125, 745}, "125"}, //clamps random lower limit, multiple of 5
        {{  1000, 435, 125, 745}, "745"}, //clamps random upper limit, multiple of 5
        {{ -1000, 435, 121, 749}, "125"}, //clamps random lower limit, not a multiple of 5
        {{  1000, 435, 121, 749}, "745"}  //clamps random upper limit, not a multiple of 5
    };

    for (const auto& [in, out] : cases) {
        EXPECT_EQ(AVRHandler::DbToString(in.value, in.zero, in.min, in.max), out) << " inputs:\n"
            << std::format("    value: {}, zero: {}, min: {}, max: {}", in.value, in.zero, in.min,
                           in.max);
    }
}
