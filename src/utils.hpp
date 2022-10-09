// Copyright (C) 2022 Vladislav Nepogodin
//
// This file is part of CachyOS kernel manager.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>       // for string
#include <string_view>  // for string_view
#include <vector>       // for vector
#include <span>         // for span

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <QString>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <alpm.h>

namespace utils {

[[nodiscard]] bool check_root() noexcept;
[[nodiscard]] auto make_multiline(const std::string_view& str, char delim = '\n') noexcept -> std::vector<std::string>;
[[nodiscard]] auto join_vec(const std::span<std::string_view>& lines, const std::string_view&& delim) noexcept -> std::string;

alpm_handle_t* parse_alpm(std::string_view root, std::string_view dbpath, alpm_errno_t* err) noexcept;
void release_alpm(alpm_handle_t* handle, alpm_errno_t* err) noexcept;

// Runs a command in a terminal, escalates using pkexec if escalate is true
int runCmdTerminal(QString cmd, bool escalate) noexcept;

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
