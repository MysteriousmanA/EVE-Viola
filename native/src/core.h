#pragma once

#include "eve_viola_native.h"

#include <string_view>

namespace eve_viola {

bool matches_game_window_title(std::wstring_view title);
EveRect fit_rect(int source_width, int source_height, int target_width, int target_height);

}  // namespace eve_viola
