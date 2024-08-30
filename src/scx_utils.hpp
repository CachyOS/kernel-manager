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
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-final-types"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <QStringList>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

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

}  // namespace scx::loader

#endif  // SCX_UTILS_HPP
