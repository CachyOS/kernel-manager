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

#include "kernel.hpp"

#include <QProcess>
#include <fmt/core.h>

std::string Kernel::version() const noexcept {
    if (!is_installed()) {
        return "Not installed";
    }
    const auto& ext_cmd = fmt::format("pacman -Q {0} | {1}", m_name, "awk '{ print $2 }'");

    QProcess pacman;
    QStringList args = {"-c", ext_cmd.c_str()};
    pacman.start("bash", args);
    if (!pacman.waitForStarted())
        return {};

    if (!pacman.waitForFinished())
        return {};

    auto output = pacman.readAll();
    return output.data();
}

// Name must be without any repo name (e.g. core/linux)
bool Kernel::is_installed() const noexcept {
    QProcess pacman;
    QStringList args = {"-Q", m_name.c_str()};
    pacman.start("pacman", args);
    if (!pacman.waitForStarted())
        return false;

    if (!pacman.waitForFinished())
        return false;

    const auto& ret_code = pacman.exitCode();
    return ret_code == 0;
}

bool Kernel::install() const noexcept {
    const auto& ext_cmd = fmt::format("yes | pacman -S --noconfirm {0} {0}-headers", m_name);

    QProcess pacman;
    QStringList args = {"-c", ext_cmd.c_str()};
    pacman.start("bash", args);
    if (!pacman.waitForStarted())
        return false;

    if (!pacman.waitForFinished())
        return false;

    const auto& ret_code = pacman.exitCode();
    return ret_code == 0;
}

bool Kernel::update() const noexcept {
    return install();
}
