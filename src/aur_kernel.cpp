// Copyright (C) 2022-2023 Vladislav Nepogodin
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

#include "aur_kernel.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <range/v3/algorithm/search.hpp>

#include <fmt/core.h>

namespace fs = std::filesystem;

namespace {

void prepare_build_environment(const std::string_view& package_name) noexcept {
    static const fs::path app_path       = utils::fix_path("~/.cache/cachyos-km");
    static const fs::path pkgbuilds_path = utils::fix_path("~/.cache/cachyos-km/aur_pkgbuilds");
    static const fs::path package_path   = utils::fix_path(fmt::format("~/.cache/cachyos-km/aur_pkgbuilds/{}", package_name));
    if (!fs::exists(app_path)) {
        fs::create_directories(app_path);
    }
    fs::current_path(app_path);

    if (!fs::exists(pkgbuilds_path)) {
        fs::create_directories(pkgbuilds_path);
    }
    fs::current_path(pkgbuilds_path);

    // Check if folder exits, but .git doesn't.
    if (fs::exists(package_path) && !fs::exists(package_path / ".git")) {
        fs::remove_all(package_path);
    }

    std::int32_t cmd_status{};
    if (!fs::exists(package_path)) {
        const auto& clone_cmd = fmt::format("git clone https://aur.archlinux.org/{}.git", package_name);
        cmd_status            = std::system(clone_cmd.c_str());
    }

    fs::current_path(package_path);
    cmd_status += std::system("git checkout --force master");
    cmd_status += std::system("git clean -fd");
    cmd_status += std::system("git pull");
    if (cmd_status != 0) {
        std::perror("prepare_build_environment");
    }
}

}  // namespace

namespace detail {

void install_aur_kernels(std::span<std::string_view> kernel_list) noexcept {
    using namespace std::literals;

    for (auto&& kernel_name : kernel_list) {
        if (auto found = ranges::search(kernel_name, "headers"sv); !found.empty()) {
            continue;
        }

        prepare_build_environment(kernel_name);

        // Run our build command!
        utils::runCmdTerminal("makepkg -sicf --cleanbuild --skipchecksums", false);
    }
}

}  // namespace detail
