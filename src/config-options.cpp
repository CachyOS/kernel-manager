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

#include "config-options.hpp"

#include <utility>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-this-capture"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "config-option-lib-cxxbridge/lib.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>

auto ConfigOptions::parse_from_file(std::string_view filepath) noexcept -> std::optional<ConfigOptions> {
    ::cachyos_km::Config rust_config_options{};
    try {
        const ::rust::Str filepath_rust(filepath.data(), filepath.size());
        rust_config_options = cachyos_km::parse_config_file(filepath_rust);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to parse config file: {}\n", e.what());
        return std::nullopt;
    }

    ConfigOptions config_options{
        .hardly_check   = rust_config_options.hardly_check,
        .per_gov_check  = rust_config_options.per_gov_check,
        .tcp_bbr3_check = rust_config_options.tcp_bbr3_check,

        .cachy_config_check        = rust_config_options.cachy_config_check,
        .nconfig_check             = rust_config_options.nconfig_check,
        .xconfig_check             = rust_config_options.xconfig_check,
        .localmodcfg_check         = rust_config_options.localmodcfg_check,
        .use_current_check         = rust_config_options.use_current_check,
        .builtin_zfs_check         = rust_config_options.builtin_zfs_check,
        .builtin_nvidia_open_check = rust_config_options.builtin_nvidia_open_check,
        .build_debug_check         = rust_config_options.build_debug_check,

        .hz_ticks_combo = std::string{rust_config_options.hz_ticks_combo},
        .tickrate_combo = std::string{rust_config_options.tickrate_combo},
        .preempt_combo  = std::string{rust_config_options.preempt_combo},
        .hugepage_combo = std::string{rust_config_options.hugepage_combo},
        .lto_combo      = std::string{rust_config_options.lto_combo},
        .cpu_opt_combo  = std::string{rust_config_options.cpu_opt_combo},

        .custom_name_edit = std::string{rust_config_options.custom_name_edit},
    };
    return std::make_optional<ConfigOptions>(std::move(config_options));
}

auto ConfigOptions::write_config_file(const ConfigOptions& config_options, std::string_view filepath) noexcept -> bool {
    const ::cachyos_km::Config rust_config_options{
        .hardly_check   = config_options.hardly_check,
        .per_gov_check  = config_options.per_gov_check,
        .tcp_bbr3_check = config_options.tcp_bbr3_check,

        .cachy_config_check        = config_options.cachy_config_check,
        .nconfig_check             = config_options.nconfig_check,
        .xconfig_check             = config_options.xconfig_check,
        .localmodcfg_check         = config_options.localmodcfg_check,
        .use_current_check         = config_options.use_current_check,
        .builtin_zfs_check         = config_options.builtin_zfs_check,
        .builtin_nvidia_open_check = config_options.builtin_nvidia_open_check,
        .build_debug_check         = config_options.build_debug_check,

        .hz_ticks_combo = rust::String(config_options.hz_ticks_combo),
        .tickrate_combo = rust::String(config_options.tickrate_combo),
        .preempt_combo  = rust::String(config_options.preempt_combo),
        .hugepage_combo = rust::String(config_options.hugepage_combo),
        .lto_combo      = rust::String(config_options.lto_combo),
        .cpu_opt_combo  = rust::String(config_options.cpu_opt_combo),

        .custom_name_edit = rust::String(config_options.custom_name_edit),
    };

    try {
        const ::rust::Str filepath_rust(filepath.data(), filepath.size());
        cachyos_km::write_config_file(rust_config_options, filepath_rust);
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to write config file: {}\n", e.what());
        return false;
    }
    return true;
}
