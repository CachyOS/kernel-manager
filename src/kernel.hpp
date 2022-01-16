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

#ifndef KERNEL_HPP
#define KERNEL_HPP

#include <string>
#include <string_view>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/search.hpp>

#pragma clang diagnostic pop
#else
#include <ranges>
namespace ranges = std::ranges;
#endif

class Kernel {
 public:
    explicit Kernel(const std::string_view& name) : m_name(name) { }
    explicit Kernel(const std::string_view& name, const std::string_view& repo) : m_name(name), m_repo(repo) { }
    explicit Kernel(const std::string_view& name, const std::string_view& repo, const std::string_view& raw) : m_name(name), m_repo(repo), m_raw(raw) { }

    consteval std::string_view category() const noexcept {
        constexpr std::string_view lto{"lto"};
        constexpr std::string_view lts{"lts"};
        constexpr std::string_view zen{"zen"};
        constexpr std::string_view hardened{"hardened"};
        constexpr std::string_view next{"next"};
        constexpr std::string_view mainline{"mainline"};
        constexpr std::string_view git{"git"};

        auto found = ranges::search(m_name, lto);
        if (!found.empty()) {
            return "lto optimized";
        }
        found = ranges::search(m_name, lts);
        if (!found.empty()) {
            return "longterm";
        }
        found = ranges::search(m_name, zen);
        if (!found.empty()) {
            return "zen-kernel";
        }
        found = ranges::search(m_name, hardened);
        if (!found.empty()) {
            return "hardened-kernel";
        }
        found = ranges::search(m_name, next);
        if (!found.empty()) {
            return "next release";
        }
        found = ranges::search(m_name, mainline);
        if (!found.empty()) {
            return "mainline branch";
        }
        found = ranges::search(m_name, git);
        if (!found.empty()) {
            return "master branch";
        }

        return "stable";
    }
    std::string version() const noexcept;

    bool is_installed() const noexcept;
    bool install() const noexcept;
    bool update() const noexcept;

    /* clang-format off */
    inline const char* get_raw() const noexcept
    { return m_raw.c_str(); }
    /* clang-format on */

    static std::vector<Kernel> get_kernels() noexcept;

 private:
    std::string m_name{};
    std::string m_repo{"local"};
    std::string m_raw{};
};

#endif  // KERNEL_HPP
