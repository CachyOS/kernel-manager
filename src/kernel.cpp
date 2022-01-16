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
#include "utils.hpp"

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

    const auto& output = pacman.readAll();
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

// Find kernel packages by finding packages which have words 'linux' and 'headers'.
// From the output of 'pacman -Sl'
// - find lines that have words: 'linux' and 'headers'
// - drop lines containing 'testing' (=testing repo, causes duplicates) and 'linux-api-headers' (=not a kernel header)
// - show the (header) package names
// Now we have names of the kernel headers.
// Then add the kernel packages to proper places and output the result.
// Then display possible kernels and headers added by the user.

// The output consists of a list of reponame and a package name formatted as: "reponame/pkgname"
// For example:
//    reponame/linux-xxx reponame/linux-xxx-headers
//    reponame/linux-yyy reponame/linux-yyy-headers
//    ...
std::vector<Kernel> Kernel::get_kernels() noexcept {
    static constexpr auto ext_cmd = "pacman -Sl | awk '{printf(\"%s/%s\\n\", $1, $2)}' | grep \"/linux[^ ]*-headers$\" | grep -Pv '^testing/|/linux-api-headers$' | sed 's|-headers$||'";

    QProcess pacman;
    pacman.start("bash", {"-c", ext_cmd});
    if (!pacman.waitForStarted())
        return {};

    if (!pacman.waitForFinished())
        return {};

    const auto& output      = pacman.readAll();
    const auto& kernel_list = utils::make_multiline(output.data());
    std::vector<Kernel> kernels{};
    kernels.reserve(kernel_list.size());
    for (const auto& kernel : kernel_list) {
        const auto& info = utils::make_multiline(kernel, false, "/");
        kernels.emplace_back(Kernel{info[1], info[0], kernel});
    }

    return kernels;
}
