#include "avr_handler.h"
#include <utility>
#include <string>
#include <gtest/gtest.h>

TEST(DbToString, ClampsDefault) {
    const int zero = 800;
    const int min = 0;
    const int max = 980;
    const int lower = min - zero;
    const int upper = max - zero;
    const char* lower_string = "00";
    const char* lowerplus_string = "01";
    const char* upper_string = "98";
    const char* upperminus_string = "97";

    std::pair<int, const char*> clamps[] = {
        {lower - 1523, lower_string},
        {lower -   10, lower_string},
        {lower -    1, lower_string},
        {lower       , lower_string},
        {lower +    1, lower_string},
        {lower +   10, lowerplus_string},
        {upper + 1523, upper_string},
        {upper +   10, upper_string},
        {upper +    1, upper_string},
        {upper       , upper_string},
        {upper -    1, upper_string},
        {upper -   10, upperminus_string}
    };

    for (const auto& [in, out] : clamps) {
        EXPECT_EQ(AVRHandler::DbToString(in), out) << " input: " << in;
    }
}
