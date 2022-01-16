#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>       // for string
#include <string_view>  // for string_view
#include <vector>       // for vector

namespace utils {

[[nodiscard]] bool is_connected() noexcept;
[[nodiscard]] bool check_root() noexcept;
[[nodiscard]] auto make_multiline(const std::string_view& str, bool reverse = false, const std::string_view&& delim = "\n") noexcept -> std::vector<std::string>;
[[nodiscard]] auto make_multiline(std::vector<std::string>& multiline, bool reverse = false, const std::string_view&& delim = "\n") noexcept -> std::string;

}  // namespace utils

#endif  // UTILS_HPP
