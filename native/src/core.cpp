#include "core.h"

#include <cwctype>

namespace eve_viola {

bool matches_game_window_title(std::wstring_view title) {
    constexpr std::wstring_view english_keyword = L"EVE";
    constexpr std::wstring_view chinese_keyword = L"星战前夜";

    for (std::size_t index = 0; index + english_keyword.size() <= title.size(); ++index) {
        if (std::towupper(title[index]) == L'E' &&
            std::towupper(title[index + 1]) == L'V' &&
            std::towupper(title[index + 2]) == L'E') {
            return true;
        }
    }
    return title.find(chinese_keyword) != std::wstring_view::npos;
}

EveRect fit_rect(int source_width, int source_height, int target_width, int target_height) {
    if (source_width <= 0 || source_height <= 0 || target_width <= 0 || target_height <= 0) {
        return {};
    }

    const auto width_limited =
        static_cast<long long>(target_width) * source_height <=
        static_cast<long long>(target_height) * source_width;

    int width = target_width;
    int height = target_height;
    if (width_limited) {
        height = static_cast<int>(static_cast<long long>(target_width) * source_height / source_width);
    } else {
        width = static_cast<int>(static_cast<long long>(target_height) * source_width / source_height);
    }

    const int left = (target_width - width) / 2;
    const int top = (target_height - height) / 2;
    return EveRect{left, top, left + width, top + height};
}

}  // namespace eve_viola
