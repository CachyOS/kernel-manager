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

#include "utils.hpp"
#include "ini.hpp"

#include <algorithm>  // for transform
#include <cstdint>    // for int32_t
#include <unistd.h>   // for getuid

#include <sys/utsname.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#include <QProcess>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace utils {

bool check_root() noexcept {
#ifdef NDEVENV
    return getuid() == 0;
#else
    return true;
#endif
}

auto make_multiline(const std::string_view& str, char delim) noexcept -> std::vector<std::string> {
    static constexpr auto functor = [](auto&& rng) {
        return std::string_view(&*rng.begin(), static_cast<size_t>(ranges::distance(rng)));
    };
    static constexpr auto second = [](auto&& rng) { return rng != ""; };

    auto&& view_res = str
        | ranges::views::split(delim)
        | ranges::views::transform(functor);

    std::vector<std::string> lines{};
    ranges::for_each(view_res | ranges::views::filter(second), [&](auto&& rng) { lines.emplace_back(rng); });
    return lines;
}

auto join_vec(const std::span<std::string_view>& lines, const std::string_view&& delim) noexcept -> std::string {
    return lines | ranges::views::join(delim) | ranges::to<std::string>();
}

int runCmdTerminal(QString cmd, bool escalate) noexcept {
    QProcess proc;
    cmd += "; read -p 'Press enter to exit'";
    auto paramlist = QStringList();
    if (escalate) {
        paramlist << "-s"
                  << "pkexec /usr/lib/cachyos-kernel-manager/rootshell.sh";
    }
    paramlist << cmd;

    proc.start("/usr/lib/cachyos-kernel-manager/terminal-helper", paramlist);
    proc.waitForFinished(-1);
    return proc.exitCode();
}

namespace {

void parse_cachedirs(alpm_handle_t* handle) noexcept {
    static constexpr auto cachedir = "/var/cache/pacman/pkg/";

    alpm_list_t* cachedirs = nullptr;
    cachedirs              = alpm_list_add(cachedirs, const_cast<void*>(reinterpret_cast<const void*>(cachedir)));
    alpm_option_set_cachedirs(handle, cachedirs);
}

void parse_includes(alpm_handle_t* handle, alpm_db_t* db, const auto& section, const auto& file) noexcept {
    const auto* archs = alpm_option_get_architectures(handle);
    const auto* arch  = reinterpret_cast<const char*>(archs->data);

    mINI::INIFile file_nested(file);
    // next, create a structure that will hold data
    mINI::INIStructure mirrorlist;

    // now we can read the file
    file_nested.read(mirrorlist);
    for (const auto& mirror : mirrorlist) {
        auto repo = mirror.second.begin()->second;
        if (repo.starts_with('/') || repo.starts_with('#')) {
            continue;
        }
        utils::replace_all(repo, "$arch", arch);
        utils::replace_all(repo, "$repo", section.c_str());
        alpm_db_add_server(db, repo.c_str());
    }
}

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
        const auto& nested  = it.second;
        if (section == ignored_repo) {
            continue;
        }
        if (section == "options") {
            for (const auto& it_nested : nested) {
                if (it_nested.first != "architecture") {
                    continue;
                }
                // add CacheDir
                const auto& archs = utils::make_multiline(it_nested.second, ' ');
                for (const auto& arch : archs) {
                    if (arch == "auto") {
                        struct utsname un;
                        uname(&un);
                        char* tmp = un.machine;
                        if (tmp != nullptr) {
                            alpm_option_add_architecture(handle, tmp);
                        }
                        continue;
                    }

                    alpm_option_add_architecture(handle, arch.c_str());
                }
            }
            continue;
        }
        auto* db = alpm_register_syncdb(handle, section.c_str(), ALPM_SIG_USE_DEFAULT);

        for (const auto& it_nested : nested) {
            const auto& param = it_nested.first;
            const auto& value = it_nested.second;
            if (param == "include") {
                parse_includes(handle, db, section, value);
            }
        }
    }
}
}  // namespace

alpm_handle_t* parse_alpm(std::string_view root, std::string_view dbpath, alpm_errno_t* err) noexcept {
    // Initialize alpm.
    alpm_handle_t* alpm_handle = alpm_initialize(root.data(), dbpath.data(), err);

    // Parse pacman config.
    parse_repos(alpm_handle);
    parse_cachedirs(alpm_handle);

    return alpm_handle;
}

void release_alpm(alpm_handle_t* handle, alpm_errno_t* err) noexcept {
    // Release libalpm handle
    alpm_release(handle);

    *err = alpm_errno(handle);
}

}  // namespace utils
