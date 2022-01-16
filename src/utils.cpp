#include "utils.hpp"

#include <algorithm>  // for transform
#include <cstdint>    // for int32_t
#include <unistd.h>   // for getuid

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#pragma clang diagnostic pop
#else
#include <ranges>
namespace ranges = std::ranges;
#endif

#ifdef NDEVENV
#include <cpr/api.h>
#include <cpr/response.h>
#include <cpr/status_codes.h>
#include <cpr/timeout.h>
#endif

namespace utils {

bool is_connected() noexcept {
#ifdef NDEVENV
    /* clang-format off */
    auto r = cpr::Get(cpr::Url{"https://duckduckgo.com"},
             cpr::Timeout{1000});
    /* clang-format on */
    return cpr::status::is_success(static_cast<std::int32_t>(r.status_code)) || cpr::status::is_redirect(static_cast<std::int32_t>(r.status_code));
#else
    return true;
#endif
}

bool check_root() noexcept {
#ifdef NDEVENV
    return getuid() == 0;
#else
    return true;
#endif
}

auto make_multiline(const std::string_view& str, bool reverse, const std::string_view&& delim) noexcept -> std::vector<std::string> {
    static constexpr auto functor = [](auto&& rng) {
        return std::string_view(&*rng.begin(), static_cast<size_t>(ranges::distance(rng)));
    };
    static constexpr auto second = [](auto&& rng) { return rng != ""; };

#if defined(__clang__)
    const auto& splitted_view = str
        | ranges::views::split(delim);
    const auto& view_res = splitted_view
        | ranges::views::transform(functor);
#else
    const auto& view_res = str
        | ranges::views::split(delim)
        | ranges::views::transform(functor);
#endif

    std::vector<std::string> lines{};
    ranges::for_each(view_res | ranges::views::filter(second), [&](auto&& rng) { lines.emplace_back(rng); });
    if (reverse) {
        ranges::reverse(lines);
    }
    return lines;
}

auto make_multiline(std::vector<std::string>& multiline, bool reverse, const std::string_view&& delim) noexcept -> std::string {
    std::string res{};
    for (const auto& line : multiline) {
        res += line;
        res += delim.data();
    }

    if (reverse) {
        ranges::reverse(res);
    }

    return res;
}

}  // namespace utils
