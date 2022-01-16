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

inline std::size_t replace_all(std::string& inout, const std::string_view& what, const std::string_view& with) {
    std::size_t count{};
    std::size_t pos{};
    while (std::string::npos != (pos = inout.find(what.data(), pos, what.length()))) {
        inout.replace(pos, what.length(), with.data(), with.length());
        pos += with.length(), ++count;
    }
    return count;
}

inline std::size_t remove_all(std::string& inout, const std::string_view& what) {
    return replace_all(inout, what, "");
}

}  // namespace utils

#endif  // UTILS_HPP
