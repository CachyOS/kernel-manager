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
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-final-types"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>

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

}  // namespace scx::loader
