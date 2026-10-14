#include "core.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    expect(eve_viola::matches_game_window_title(L"EVE Online"), "uppercase EVE title matches");
    expect(eve_viola::matches_game_window_title(L"eve - character"), "lowercase eve title matches");
    expect(eve_viola::matches_game_window_title(L"My EvE Client"), "mixed-case EvE title matches");
    expect(
        eve_viola::matches_game_window_title(L"星战前夜：晨曦[Infinity] - 噩梦级"),
        "Chinese EVE title matches");
    expect(!eve_viola::matches_game_window_title(L"Calculator"), "unrelated title does not match");
    expect(!eve_viola::matches_game_window_title(L""), "empty title does not match");

    const auto widescreen = eve_viola::fit_rect(1920, 1080, 800, 600);
    expect(widescreen.left == 0, "16:9 left is centered");
    expect(widescreen.top == 75, "16:9 top letterbox is 75");
    expect(widescreen.right == 800, "16:9 width fills target");
    expect(widescreen.bottom == 525, "16:9 bottom letterbox is 75");

    const auto portrait = eve_viola::fit_rect(1000, 2000, 600, 400);
    expect(portrait.left == 200, "portrait left pillarbox is 200");
    expect(portrait.top == 0, "portrait top fills target");
    expect(portrait.right == 400, "portrait right pillarbox is 200");
    expect(portrait.bottom == 400, "portrait height fills target");

    const auto invalid = eve_viola::fit_rect(0, 100, 600, 400);
    expect(invalid.left == 0 && invalid.top == 0 && invalid.right == 0 && invalid.bottom == 0,
           "invalid source returns an empty rectangle");

    return 0;
}
