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

#include "scx_utils.hpp"

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

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#include "config-option-lib-cxxbridge/scx_loader_config.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>

namespace {

auto convert_rust_vec_string(auto&& rust_vec) -> std::vector<std::string> {
    std::vector<std::string> res_vec{};
    res_vec.reserve(rust_vec.size());
    for (auto&& rust_vec_el : rust_vec) {
        res_vec.emplace_back(std::string{rust_vec_el});
    }
    return res_vec;
}

auto convert_std_vec_into_stringlist(std::vector<std::string>&& std_vec) -> QStringList {
    QStringList flags;
    flags.reserve(static_cast<qsizetype>(std_vec.size()));
    for (auto&& vec_el : std_vec) {
        flags << QString::fromStdString(vec_el);
    }
    return flags;
}

}  // namespace

namespace scx::loader {

auto get_supported_scheds() noexcept -> std::optional<QStringList> {
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.scx.Loader",
        "/org/scx/Loader",
        "org.freedesktop.DBus.Properties",
        "Get");
    message << "org.scx.Loader" << "SupportedSchedulers";
    QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        fmt::print(stderr, "Failed to get supported schedulers: {}\n", reply.errorMessage().toStdString());
        return std::nullopt;
    }

    return reply.arguments().at(0).value<QDBusVariant>().variant().toStringList();
}

auto get_current_scheduler() noexcept -> std::optional<QString> {
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.scx.Loader",
        "/org/scx/Loader",
        "org.freedesktop.DBus.Properties",
        "Get");
    message << "org.scx.Loader" << "CurrentScheduler";
    QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        fmt::print(stderr, "Failed to get current scheduler: {}\n", reply.errorMessage().toStdString());
        return std::nullopt;
    }

    return reply.arguments().at(0).value<QDBusVariant>().variant().toString();
}

auto switch_scheduler_with_args(std::string_view scx_sched, QStringList sched_args) noexcept -> bool {
    QDBusMessage message = QDBusMessage::createMethodCall(
        "org.scx.Loader",
        "/org/scx/Loader",
        "org.scx.Loader",
        "SwitchSchedulerWithArgs");
    message << QString::fromStdString(std::string{scx_sched}) << sched_args;
    QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        fmt::print(stderr, "Failed to switch scheduler with args: {}\n", reply.errorMessage().toStdString());
        return false;
    }
    return true;
}

auto Config::init_config(std::string_view filepath) noexcept -> std::optional<Config> {
    try {
        const ::rust::Str filepath_rust(filepath.data(), filepath.size());
        auto&& config = Config(scx_loader::init_config_file(filepath_rust));
        return std::make_optional<Config>(std::move(config));
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to parse init config: {}\n", e.what());
    }
    return std::nullopt;
}

auto Config::scx_flags_for_mode(std::string_view scx_sched, SchedMode sched_mode) noexcept -> std::optional<QStringList> {
    try {
        const ::rust::Str scx_sched_rust(scx_sched.data(), scx_sched.size());
        auto rust_vec  = m_config->get_scx_flags_for_mode(scx_sched_rust, static_cast<std::uint32_t>(sched_mode));
        auto flags_vec = convert_rust_vec_string(std::move(rust_vec));
        return convert_std_vec_into_stringlist(std::move(flags_vec));
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to get scx flag for the mode: {}\n", e.what());
    }
    return std::nullopt;
}

auto Config::set_scx_sched_with_mode(std::string_view scx_sched, SchedMode sched_mode) noexcept -> bool {
    try {
        const ::rust::Str scx_sched_rust(scx_sched.data(), scx_sched.size());
        m_config->set_scx_sched_with_mode(scx_sched_rust, static_cast<std::uint32_t>(sched_mode));
        return true;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to set default scx scheduler with mode: {}\n", e.what());
    }
    return false;
}

auto Config::write_config_file(std::string_view filepath) noexcept -> bool {
    try {
        const ::rust::Str filepath_rust(filepath.data(), filepath.size());
        m_config->write_config_file(filepath_rust);
        return true;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to set default scx scheduler with mode: {}\n", e.what());
    }
    return false;
}

}  // namespace scx::loader
