// Copyright (C) 2024-2025 Vladislav Nepogodin
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

#ifndef CONFIGOPTIONS_HPP_
#define CONFIGOPTIONS_HPP_

#include <optional>     // for optional
#include <string>       // for string
#include <string_view>  // for string_view

struct ConfigOptions {
    bool hardly_check{};
    bool per_gov_check{};
    bool tcp_bbr3_check{};

    bool cachy_config_check{};
    bool nconfig_check{};
    bool xconfig_check{};
    bool localmodcfg_check{};
    bool use_current_check{};
    bool builtin_zfs_check{};
    bool builtin_nvidia_open_check{};
    bool build_debug_check{};

    std::string hz_ticks_combo{};
    std::string tickrate_combo{};
    std::string preempt_combo{};
    std::string hugepage_combo{};
    std::string lto_combo{};
    std::string cpu_opt_combo{};

    std::string custom_name_edit{};

    static auto parse_from_file(std::string_view filepath) noexcept -> std::optional<ConfigOptions>;
    static auto write_config_file(const ConfigOptions& config_options, std::string_view filepath) noexcept -> bool;
};

#endif  // CONFIGOPTIONS_HPP_
