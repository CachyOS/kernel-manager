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
#include "ini.hpp"
#include "utils.hpp"

#include <QProcess>
#include <fmt/core.h>

namespace {
void parse_repos(alpm_handle_t* handle) noexcept {
    static constexpr auto pacman_conf_path = "/etc/pacman.conf";
    static constexpr auto ignored_repo     = "testing";

    mINI::INIFile file(pacman_conf_path);
    // next, create a structure that will hold data
    mINI::INIStructure ini;

    // now we can read the file
    file.read(ini);
    for (const auto& it : ini) {
        const auto& section = it.first;
        if ((section == "options") || (section == ignored_repo)) {
            continue;
        }
        alpm_register_syncdb(handle, section.c_str(), ALPM_SIG_USE_DEFAULT);
    }
}
}  // namespace

std::string Kernel::version() const noexcept {
    const char* sync_pkg_ver = alpm_pkg_get_version(m_pkg);
    if (!is_installed()) {
        return sync_pkg_ver;
    }

    auto* db                  = alpm_get_localdb(m_handle);
    auto* local_pkg           = alpm_db_get_pkg(db, m_name.c_str());
    const char* local_pkg_ver = alpm_pkg_get_version(local_pkg);
    const int32_t ret         = alpm_pkg_vercmp(local_pkg_ver, sync_pkg_ver);
    if (ret == 1) {
        return fmt::format("∨{}", local_pkg_ver);
    } else if (ret == -1) {
        return fmt::format("∧{}", sync_pkg_ver);
    }

    return sync_pkg_ver;
}

// Name must be without any repo name (e.g. core/linux)
bool Kernel::is_installed() const noexcept {
    auto* db  = alpm_get_localdb(m_handle);
    auto* pkg = alpm_db_get_pkg(db, m_name.c_str());

    return pkg != NULL;
}

bool Kernel::install() const noexcept {
    return alpm_add_pkg(m_handle, m_pkg) == 0;
    //    const auto& ext_cmd = fmt::format("yes | pacman -S --noconfirm {0} {0}-headers", m_name);

    //    QProcess pacman;
    //    QStringList args = {"-c", ext_cmd.c_str()};
    //    pacman.start("bash", args);
    //    if (!pacman.waitForStarted())
    //        return false;

    //    if (!pacman.waitForFinished())
    //        return false;

    //    const auto& ret_code = pacman.exitCode();
    //    return ret_code == 0;
}

bool Kernel::remove() const noexcept {
    return alpm_remove_pkg(m_handle, m_pkg) == 0;
    //    const auto& ext_cmd = fmt::format("yes | pacman -Rsn --noconfirm {0} {0}-headers", m_name);

    //    QProcess pacman;
    //    QStringList args = {"-c", ext_cmd.c_str()};
    //    pacman.start("bash", args);
    //    if (!pacman.waitForStarted())
    //        return false;

    //    if (!pacman.waitForFinished())
    //        return false;

    //    const auto& ret_code = pacman.exitCode();
    //    return ret_code == 0;
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
std::vector<Kernel> Kernel::get_kernels(alpm_handle_t* handle) noexcept {
    static constexpr std::string_view ignored_pkg  = "linux-api-headers";
    static constexpr std::string_view replace_part = "-headers";
    std::vector<Kernel> kernels{};

    parse_repos(handle);

    auto* dbs = alpm_get_syncdbs(handle);
    for (alpm_list_t* i = dbs; i != nullptr; i = i->next) {
        static constexpr auto needle = "linux[^ ]*-headers";
        alpm_list_t* needles         = nullptr;
        alpm_list_t* ret_list        = nullptr;
        needles                      = alpm_list_add(needles, const_cast<void*>(reinterpret_cast<const void*>(needle)));

        auto* db            = reinterpret_cast<alpm_db_t*>(i->data);
        const char* db_name = alpm_db_get_name(db);
        alpm_db_search(db, needles, &ret_list);

        for (alpm_list_t* j = ret_list; j != nullptr; j = j->next) {
            auto* pkg            = reinterpret_cast<alpm_pkg_t*>(j->data);
            std::string pkg_name = alpm_pkg_get_name(pkg);

            const auto& found = ranges::search(pkg_name, ignored_pkg);
            if (!found.empty()) {
                continue;
            }

            utils::remove_all(pkg_name, replace_part);

            pkg = alpm_db_get_pkg(db, pkg_name.c_str());
            kernels.emplace_back(Kernel{handle, pkg, db_name, fmt::format("{}/{}", db_name, pkg_name)});
        }

        alpm_list_free(needles);
        alpm_list_free(ret_list);
    }

    return kernels;
}
