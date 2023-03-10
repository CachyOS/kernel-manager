// Copyright (C) 2022-2023 Vladislav Nepogodin
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
#include "aur_kernel.hpp"
#include "utils.hpp"

#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <memory>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <range/v3/algorithm/find_if.hpp>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/compile.h>
#include <fmt/core.h>

namespace {

#ifdef PKG_DUMMY_IMPL
static std::vector<std::string_view> g_aur_kernel_install_list{};
static std::vector<std::string_view> g_kernel_install_list{};
static std::vector<std::string_view> g_kernel_removal_list{};
#endif

}  // namespace

namespace fs = std::filesystem;

std::string Kernel::version() noexcept {
#ifdef ENABLE_AUR_KERNELS
    if (m_repo == "aur") { return m_version; }
#endif
    const char* sync_pkg_ver = alpm_pkg_get_version(m_pkg);
    if (!is_installed()) {
        return sync_pkg_ver;
    }

    auto* db                  = alpm_get_localdb(m_handle);
    auto* local_pkg           = alpm_db_get_pkg(db, m_name.c_str());
    const char* local_pkg_ver = alpm_pkg_get_version(local_pkg);
    const int32_t ret         = alpm_pkg_vercmp(local_pkg_ver, sync_pkg_ver);
    if (ret == 1) {
        return fmt::format(FMT_COMPILE("∨{}"), local_pkg_ver);
    } else if (ret == -1) {
        m_update = true;
        return fmt::format(FMT_COMPILE("∧{}"), sync_pkg_ver);
    }

    return sync_pkg_ver;
}

// Name must be without any repo name (e.g. core/linux)
bool Kernel::is_installed() const noexcept {
    auto* db  = alpm_get_localdb(m_handle);
    auto* pkg = alpm_db_get_pkg(db, m_name.c_str());

    return pkg != nullptr;
}

bool Kernel::install() const noexcept {
#ifdef PKG_DUMMY_IMPL
#ifdef ENABLE_AUR_KERNELS
    if (m_repo == "aur") {
        g_aur_kernel_install_list.insert(g_aur_kernel_install_list.end(), {m_name.c_str()});
        return true;
    }
#endif
    const char* pkg_name    = alpm_pkg_get_name(m_pkg);
    const char* pkg_headers = alpm_pkg_get_name(m_headers);
    g_kernel_install_list.insert(g_kernel_install_list.end(), {pkg_name, pkg_headers});
    return true;
#else
    fmt::print(stderr, "installing ({})...\n", alpm_pkg_get_name(m_pkg));
    if (alpm_add_pkg(m_handle, m_pkg) != 0) {
        return false;
    }
    fmt::print(stderr, "installing headers ({})...\n", alpm_pkg_get_name(m_headers));
    return alpm_add_pkg(m_handle, m_headers) == 0;
#endif
}

bool Kernel::remove() const noexcept {
#ifdef PKG_DUMMY_IMPL
    const char* pkg_headers = alpm_pkg_get_name(m_headers);
    g_kernel_removal_list.insert(g_kernel_removal_list.end(), {m_name, pkg_headers});
    return true;
#else
    auto* db  = alpm_get_localdb(m_handle);
    auto* pkg = alpm_db_get_pkg(db, m_name.c_str());

    fmt::print(stderr, "installing ({})...\n", alpm_pkg_get_name(m_pkg));
    if (alpm_remove_pkg(m_handle, pkg) != 0) {
        return false;
    }
    fmt::print(stderr, "installing headers ({})...\n", alpm_pkg_get_name(m_headers));
    auto* headers = alpm_db_get_pkg(db, alpm_pkg_get_name(m_headers));
    return alpm_remove_pkg(m_handle, headers) == 0;
#endif
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

    auto* dbs      = alpm_get_syncdbs(handle);
    auto* local_db = alpm_get_localdb(handle);
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
            const auto& found    = ranges::search(pkg_name, ignored_pkg);
            if (!found.empty()) {
                continue;
            }
            alpm_pkg_t* headers = alpm_db_get_pkg(db, pkg_name.c_str());

            utils::remove_all(pkg_name, replace_part);
            pkg = alpm_db_get_pkg(db, pkg_name.c_str());

            // Skip if the actual kernel package is not found
            if (!pkg) {
                continue;
            }

            auto kernel_obj = Kernel{handle, pkg, headers, db_name, fmt::format(FMT_COMPILE("{}/{}"), db_name, pkg_name)};

            auto* local_pkg = alpm_db_get_pkg(local_db, pkg_name.c_str());
            if (local_pkg) {
                const char* pkg_installed_db = alpm_pkg_get_installed_db(local_pkg);
                if (pkg_installed_db != nullptr) {
                    kernel_obj.m_installed_db = pkg_installed_db;
                }
            }

            kernels.emplace_back(std::move(kernel_obj));
        }

        alpm_list_free(needles);
        alpm_list_free(ret_list);
    }

#ifdef ENABLE_AUR_KERNELS
    bool is_paru_installed{true};
    if (!fs::exists("/sbin/paru") && !fs::exists("/sbin/awk")) {
        fmt::print(stderr, "Paru & AWK are not installed! Disabling AUR kernels support\n");
        is_paru_installed = false;
    }

    if (!kernels.empty() && is_paru_installed) {
        auto&& aur_kernels_headers = utils::make_multiline(utils::exec("paru --aur -Sl | grep ' linux[^ ]*-headers' | awk '{print $2}'"));

        for (auto&& aur_kernel_header : aur_kernels_headers) {
            auto&& aur_kernel = std::string{aur_kernel_header};
            utils::replace_all(aur_kernel, "-headers", "");
            if (ranges::find_if(kernels, [&](auto& kernel){ return kernel.m_name == aur_kernel; }) != kernels.end()) {
                continue;
            }
            Kernel kernel_obj{};

            kernel_obj.m_handle = handle;
            kernel_obj.m_repo = "aur";
            kernel_obj.m_name = aur_kernel;
            kernel_obj.m_name_headers = aur_kernel_header;
            kernel_obj.m_version = "unknown-version";
            kernel_obj.m_raw = fmt::format("aur/{}", aur_kernel);

            kernels.emplace_back(std::move(kernel_obj));
        }
    }
#endif

    return kernels;
}

#ifdef PKG_DUMMY_IMPL

void Kernel::commit_transaction() noexcept {
#ifdef ENABLE_AUR_KERNELS
    if (!g_aur_kernel_install_list.empty()) {
        detail::install_aur_kernels(g_aur_kernel_install_list);
        g_aur_kernel_install_list.clear();
    }
#endif
    if (!g_kernel_install_list.empty()) {
        const auto& packages_install = utils::join_vec(g_kernel_install_list, " ");
        utils::runCmdTerminal(fmt::format(FMT_COMPILE("pacman -S --needed {}"), packages_install).c_str(), true);
        g_kernel_install_list.clear();
    }

    if (!g_kernel_removal_list.empty()) {
        const auto& packages_remove = utils::join_vec(g_kernel_removal_list, " ");
        utils::runCmdTerminal(fmt::format(FMT_COMPILE("pacman -Rsn {}"), packages_remove).c_str(), true);
        g_kernel_removal_list.clear();
    }
}
#endif
