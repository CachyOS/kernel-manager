// Copyright (C) 2022-2024 Vladislav Nepogodin
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

#include <cerrno>   // for errno
#include <cstdio>   // for fopen, fclose, fread, fseek, ftell, SEEK_END, SEEK_SET
#include <cstdlib>  // for system

#include <filesystem>

#include <fmt/core.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#include <glib.h>

#include <QProcess>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace utils {

auto read_whole_file(const std::string_view& filepath) noexcept -> std::string {
    // Use std::fopen because it's faster than std::ifstream
    auto* file = std::fopen(filepath.data(), "rb");
    if (file == nullptr) {
        fmt::print(stderr, "[READWHOLEFILE] '{}' read failed: {}\n", filepath, std::strerror(errno));
        return {};
    }

    std::fseek(file, 0u, SEEK_END);
    const std::size_t size = static_cast<std::size_t>(std::ftell(file));
    std::fseek(file, 0u, SEEK_SET);

    std::string buf;
    buf.resize(size);

    const std::size_t read = std::fread(buf.data(), sizeof(char), size, file);
    if (read != size) {
        fmt::print(stderr, "[READWHOLEFILE] '{}' read failed: {}\n", filepath, std::strerror(errno));
        return {};
    }
    std::fclose(file);

    return buf;
}

bool write_to_file(const std::string_view& filepath, const std::string_view& data) noexcept {
    std::ofstream file{filepath.data()};
    if (!file.is_open()) {
        fmt::print(stderr, "[WRITE_TO_FILE] '{}' open failed: {}\n", filepath, std::strerror(errno));
        return false;
    }
    file << data;
    return true;
}

// https://github.com/sheredom/subprocess.h
// https://gist.github.com/konstantint/d49ab683b978b3d74172
// https://github.com/arun11299/cpp-subprocess/blob/master/subprocess.hpp#L1218
// https://stackoverflow.com/questions/11342868/c-interface-for-interactive-bash
// https://github.com/hniksic/rust-subprocess
std::string exec(const std::string_view& command) noexcept {
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.data(), "r"), pclose);

    if (!pipe) {
        fmt::print(stderr, "popen failed! '{}'\n", command);
        return "-1";
    }

    std::string result{};
    std::array<char, 128> buffer{};
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }

    if (result.ends_with('\n')) {
        result.pop_back();
    }

    return result;
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

std::string fix_path(std::string&& path) noexcept {
    /* clang-format off */
    if (path[0] != '~') { return path; }
    /* clang-format on */
    utils::replace_all(path, "~", g_get_home_dir());
    return path;
}

alpm_handle_t* parse_alpm(std::string_view root, std::string_view dbpath, alpm_errno_t* err) noexcept {
    // Initialize alpm.
    alpm_handle_t* alpm_handle = alpm_initialize(root.data(), dbpath.data(), err);

    // Parse pacman config.
    static constexpr auto pacman_conf_path = "/etc/pacman.conf";
    static constexpr auto ignored_repo     = "testing";

    mINI::INIFile file(pacman_conf_path);
    // next, create a structure that will hold data
    mINI::INIStructure ini;

    // now we can read the file
    file.read(ini);
    for (const auto& it : ini) {
        const auto& section = it.first;
        if (section == ignored_repo || section == "options") {
            continue;
        }
        [[maybe_unused]] auto* db = alpm_register_syncdb(alpm_handle, section.c_str(), ALPM_SIG_USE_DEFAULT);
    }

    return alpm_handle;
}

std::int32_t release_alpm(alpm_handle_t* handle, alpm_errno_t* err) noexcept {
    // Release libalpm handle
    std::int32_t ret = alpm_release(handle);

    *err = alpm_errno(handle);

    return ret;
}

void prepare_build_environment() noexcept {
    static const fs::path app_path       = utils::fix_path("~/.cache/cachyos-km");
    static const fs::path pkgbuilds_path = utils::fix_path("~/.cache/cachyos-km/pkgbuilds");
    if (!fs::exists(app_path)) {
        fs::create_directories(app_path);
    }

    fs::current_path(app_path);

    // Check if folder exits, but .git doesn't.
    if (fs::exists(pkgbuilds_path) && !fs::exists(pkgbuilds_path / ".git")) {
        fs::remove_all(pkgbuilds_path);
    }

    std::int32_t cmd_status{};
    if (!fs::exists(pkgbuilds_path)) {
        cmd_status = std::system("git clone https://github.com/cachyos/linux-cachyos.git pkgbuilds");
    }

    fs::current_path(pkgbuilds_path);
    cmd_status += std::system("git checkout --force master");
    cmd_status += std::system("git clean -fd");
    cmd_status += std::system("git pull");
    if (cmd_status != 0) {
        std::perror("prepare_build_environment");
    }
}

}  // namespace utils
