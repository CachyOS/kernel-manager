// Copyright (C) 2024 Vladislav Nepogodin
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

#ifndef SCX_UTILS_HPP
#define SCX_UTILS_HPP

#include <optional>
#include <string_view>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-this-capture"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-final-types"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <QStringList>

#include "rust/cxx.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace scx_loader {
struct Config;
}

namespace scx {

enum class SchedMode : std::uint8_t {
    /// Default values for the scheduler
    Auto = 0,
    /// Applies flags for better gaming experience
    Gaming = 1,
    /// Applies flags for lower power usage
    PowerSave = 2,
    /// Starts scheduler in low latency mode
    LowLatency = 3,
};

}  // namespace scx

namespace scx::loader {

// Gets supported schedulers by scx_loader
auto get_supported_scheds() noexcept -> std::optional<QStringList>;

// Gets currently running scheduler by scx_loader
auto get_current_scheduler() noexcept -> std::optional<QString>;

// Switches scheduler with specified args
auto switch_scheduler_with_args(std::string_view scx_sched, QStringList sched_args) noexcept -> bool;

/// @brief Manages configuration of scx_loader.
///
/// This structure holds pointer to object from Rust code, which represents
/// the actual Config structure.
class Config {
 public:
    /// @brief Initializes config from config path, if the file
    /// doesn't exist config with default values will be created.
    static auto init_config(std::string_view filepath) noexcept -> std::optional<Config>;

    /// @brief Writes the config to the file.
    auto write_config_file(std::string_view filepath) noexcept -> bool;

    /// @brief Returns the scx flags for the given sched mode.
    auto scx_flags_for_mode(std::string_view scx_sched, SchedMode sched_mode) noexcept -> std::optional<QStringList>;

    /// @brief Sets the default scheduler with default mode.
    auto set_scx_sched_with_mode(std::string_view scx_sched, SchedMode sched_mode) noexcept -> bool;

    // explicitly deleted
    Config() = delete;

 private:
    explicit Config(::rust::Box<::scx_loader::Config>&& config) : m_config(std::move(config)) { }

    /// @brief A pointer to the Config structure from Rust code.
    ::rust::Box<::scx_loader::Config> m_config;
};

/// @brief A unique pointer to a Config object.
using ConfigPtr = std::unique_ptr<Config>;

}  // namespace scx::loader

#endif  // SCX_UTILS_HPP
