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

#include "conf-window.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdlib>

#include <filesystem>
#include <unordered_map>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <glib.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>
#include <fmt/compile.h>

#include <QStringList>

namespace fs = std::filesystem;

static const std::unordered_map<std::string_view, std::string_view> default_option_map = {
    {"nconfig", "_makenconfig="},
    {"numa", "_NUMAdisable=y"},
    {"hardly", "_cc_harder="},
    {"per_gov", "_per_gov=y"},
    {"tcp_bbr2", "_tcp_bbr2=y"},
    {"HZ_ticks", "_HZ_ticks=750"},
    {"tickrate", "_tickrate=full"},
    {"mqdeadline", "_mq_deadline_disable=y"},
    {"kyber", "_kyber_disable=y"},
    {"lru", "_lru_enable=y"},
    {"damon", "_damon="},
    {"spf", "_spf_enable=y"},
    {"lrng", "_lrng_enable=y"},
    {"cpu_opt", "_processor_opt="},
    {"auto_optim", "_use_auto_optimization=y"},
    {"debug", "_disable_debug=y"},
    {"zstd_comp", "_zstd_compression=y"},
    {"nf_cone", "_nf_cone=y"},
    {"lto", "_use_llvm_lto="},
    {"builtin_zfs", "_build_zfs="}};

static const std::unordered_map<std::string_view, std::string_view> option_map = {
    {"nconfig", "_makenconfig"},
    {"numa", "_NUMAdisable"},
    {"hardly", "_cc_harder"},
    {"per_gov", "_per_gov"},
    {"tcp_bbr2", "_tcp_bbr2"},
    {"HZ_ticks", "_HZ_ticks"},
    {"tickrate", "_tickrate"},
    {"mqdeadline", "_mq_deadline_disable"},
    {"kyber", "_kyber_disable"},
    {"lru", "_lru_enable"},
    {"damon", "_damon"},
    {"spf", "_spf_enable"},
    {"lrng", "_lrng_enable"},
    {"cpu_opt", "_processor_opt"},
    {"auto_optim", "_use_auto_optimization"},
    {"debug", "_disable_debug"},
    {"zstd_comp", "_zstd_compression"},
    {"nf_cone", "_nf_cone"},
    {"lto", "_use_llvm_lto"},
    {"builtin_zfs", "_build_zfs"}};

[[gnu::pure]] constexpr const char* get_kernel_name(size_t index) noexcept {
    constexpr std::array kernel_names{"bmq", "bore", "cacule", "cfs", "hardened", "pds", "rc", "tt"};
    return kernel_names[index];
}

[[gnu::pure]] constexpr const char* get_hz_tick(size_t index) noexcept {
    constexpr std::array hz_ticks{"1000", "750", "600", "500"};
    return hz_ticks[index];
}

[[gnu::pure]] constexpr const char* get_tickless_mode(size_t index) noexcept {
    constexpr std::array tickless_modes{"full", "idle", "perodic"};
    return tickless_modes[index];
}

[[gnu::pure]] constexpr const char* get_lto_mode(size_t index) noexcept {
    constexpr std::array lto_modes{"no", "full", "thin"};
    return lto_modes[index];
}

[[gnu::pure]] constexpr const char* get_cpu_opt_mode(size_t index) noexcept {
    constexpr std::array cpu_opt_modes{"manual", "generic", "native_amd", "native_intel", "zen", "zen2", "zen3", "sandybridge", "ivybridge", "haswell", "icelake", "tigerlake", "alderlake"};
    return cpu_opt_modes[index];
}

constexpr std::string_view get_kernel_name_path(std::string_view kernel_name) noexcept {
    if (kernel_name == "bmq") {
        return "linux-cachyos-bmq";
    } else if (kernel_name == "bore") {
        return "linux-cachyos-bore";
    } else if (kernel_name == "cacule") {
        return "linux-cachyos-cacule";
    } else if (kernel_name == "cfs") {
        return "linux-cachyos-cfs";
    } else if (kernel_name == "hardened") {
        return "linux-cachyos-hardened";
    } else if (kernel_name == "pds") {
        return "linux-cachyos-pds";
    } else if (kernel_name == "rc") {
        return "linux-cachyos-rc";
    } else if (kernel_name == "tt") {
        return "linux-cachyos-tt";
    }
    return "linux-cachyos";
}

std::string fix_path(std::string&& path) noexcept {
    if (path[0] != '~') { return path; }
    utils::replace_all(path, "~", g_get_home_dir());
    return path;
}

void prepare_build_environment() noexcept {
    static const fs::path app_path       = fix_path("~/.cache/cachyos-km");
    static const fs::path pkgbuilds_path = fix_path("~/.cache/cachyos-km/pkgbuilds");
    if (!fs::exists(app_path)) {
        fs::create_directories(app_path);
    }

    fs::current_path(app_path);

    std::int32_t cmd_status{};
    if (!fs::exists(pkgbuilds_path)) {
        cmd_status = std::system("git clone https://github.com/cachyos/linux-cachyos.git pkgbuilds");
    }

    fs::current_path(app_path / "pkgbuilds");
    cmd_status += std::system("git checkout --force master");
    cmd_status += std::system("git clean -fd");
    cmd_status += std::system("git pull");
    if (cmd_status != 0) {
        std::perror("prepare_build_environment");
    }
}

void execute_sed(std::string_view option, std::string_view value) noexcept {
    const auto& sed_cmd = fmt::format(FMT_COMPILE("sed -i \"s/{}/{}={}/\" PKGBUILD"), default_option_map.at(option), option_map.at(option), value);
    if (std::system(sed_cmd.c_str()) != 0) {
        std::perror("execute_sed");
    }
}

const char* convert_checkstate(QCheckBox* checkbox) noexcept {
    return (checkbox->checkState() == Qt::Checked) ? "y" : "n";
}

void child_watch_cb(GPid pid, [[maybe_unused]] gint status, gpointer user_data) {
#if !defined(NDEBUG)
    g_message("Child %" G_PID_FORMAT " exited %s", pid,
        g_spawn_check_wait_status(status, nullptr) ? "normally" : "abnormally");
#endif

    // Free any resources associated with the child here, such as I/O channels
    // on its stdout and stderr FDs. If you have no code to put in the
    // child_watch_cb() callback, you can remove it and the g_child_watch_add()
    // call, but you must also remove the G_SPAWN_DO_NOT_REAP_CHILD flag,
    // otherwise the child process will stay around as a zombie until this
    // process exits.
    g_spawn_close_pid(pid);

    auto* data = static_cast<bool*>(user_data);
    *data      = false;
}

void run_cmd_async(std::string&& cmd, bool* data) {
    cmd += "; read -p 'Press enter to exit'";
    const gchar* const argv[] = {"/usr/lib/cachyos-kernel-manager/terminal-helper", cmd.c_str(), nullptr};
    gint child_stdout, child_stderr;
    GPid child_pid;
    g_autoptr(GError) error = nullptr;

    // Spawn child process.
    g_spawn_async_with_pipes(nullptr, const_cast<gchar**>(argv), nullptr, G_SPAWN_DO_NOT_REAP_CHILD, nullptr,
        nullptr, &child_pid, nullptr, &child_stdout,
        &child_stderr, &error);
    if (error != nullptr) {
        fmt::print(stderr, "Spawning child failed: {}", error->message);
        return;
    }
    // Add a child watch function which will be called when the child process
    // exits.
    g_child_watch_add(child_pid, child_watch_cb, data);
}

ConfWindow::ConfWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    // Selecting the CPU scheduler
    QStringList kernel_names;
    kernel_names << "BMQ - BitMap Queue CPU scheduler"
                 << "Bore - Burst-Oriented Response Enhancer"
                 << "Cacule - CacULE scheduler"
                 << "CFS - Completely Fair Scheduler"
                 << "Hardened - Hardened kernel with the BORE Scheduler"
                 << "PDS - Priority and Deadline based Skip list multiple queue CPU scheduler"
                 << "RC - Release Candidate"
                 << "TT - Task Type Scheduler";
    m_ui->comboBox->addItems(kernel_names);
    m_ui->comboBox->setCurrentIndex(1);

    // Setting default options
    m_ui->numacheck->setCheckState(Qt::Checked);
    m_ui->perfgoverncheck->setCheckState(Qt::Checked);
    m_ui->tcpbbrcheck->setCheckState(Qt::Checked);

    QStringList hz_ticks;
    hz_ticks << "1000HZ"
             << "750Hz"
             << "600Hz"
             << "500Hz";
    m_ui->hztickscomboBox->addItems(hz_ticks);
    m_ui->hztickscomboBox->setCurrentIndex(1);

    QStringList tickless_modes;
    tickless_modes << "Full"
                   << "Idle"
                   << "Periodic";
    m_ui->ticklesscomboBox->addItems(tickless_modes);

    m_ui->mqdiocheck->setCheckState(Qt::Checked);
    m_ui->kybercheck->setCheckState(Qt::Checked);
    m_ui->lrucheck->setCheckState(Qt::Checked);
    m_ui->spfcheck->setCheckState(Qt::Checked);
    m_ui->lrngcheck->setCheckState(Qt::Checked);

    QStringList cpu_optims;
    cpu_optims << "Disabled"
               << "Generic"
               << "Native AMD"
               << "Native Intel"
               << "Zen" << "Zen2" << "Zen3"
               << "Sandy Bridge" << "Ivy Bridge" << "Haswell"
               << "Icelake" << "Tiger Lake" << "Alder Lake";
    m_ui->processor_optcomboBox->addItems(cpu_optims);

    m_ui->autooptimcheck->setCheckState(Qt::Checked);
    m_ui->debugcheck->setCheckState(Qt::Checked);
    m_ui->zstcompcheck->setCheckState(Qt::Checked);
    m_ui->nfconecheck->setCheckState(Qt::Checked);

    QStringList lto_modes;
    lto_modes << "No"
              << "Full"
              << "Thin";
    m_ui->ltocomboBox->addItems(lto_modes);

    // Connect buttons signal
    connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(on_execute()));
}

void ConfWindow::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
}

void ConfWindow::on_cancel() noexcept {
    close();
}

void ConfWindow::on_execute() noexcept {
    // Skip execution of the build, if already one is running
    if (m_running) { return; }
    m_running = true;

    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(m_ui->comboBox->currentIndex())));
    prepare_build_environment();
    fs::current_path(cpusched_path);

    // Execute 'sed' with checkboxes values
    execute_sed("numa", convert_checkstate(m_ui->numacheck));
    execute_sed("per_gov", convert_checkstate(m_ui->perfgoverncheck));
    execute_sed("tcp_bbr2", convert_checkstate(m_ui->tcpbbrcheck));
    execute_sed("mqdeadline", convert_checkstate(m_ui->mqdiocheck));
    execute_sed("kyber", convert_checkstate(m_ui->kybercheck));
    execute_sed("lru", convert_checkstate(m_ui->lrucheck));
    execute_sed("spf", convert_checkstate(m_ui->spfcheck));
    execute_sed("lrng", convert_checkstate(m_ui->lrngcheck));
    execute_sed("auto_optim", convert_checkstate(m_ui->autooptimcheck));
    execute_sed("debug", convert_checkstate(m_ui->debugcheck));
    execute_sed("zstd_comp", convert_checkstate(m_ui->zstcompcheck));
    execute_sed("nf_cone", convert_checkstate(m_ui->nfconecheck));

    const auto& is_nconfig_enabled = (m_ui->nconfigcheck->checkState() == Qt::Checked);
    const auto& is_hardly_enabled = (m_ui->hardlycheck->checkState() == Qt::Checked);
    const auto& is_damon_enabled = (m_ui->damoncheck->checkState() == Qt::Checked);
    const auto& is_builtin_zfs_enabled = (m_ui->builtin_zfscheck->checkState() == Qt::Checked);
    if (is_nconfig_enabled) {
        execute_sed("nconfig", "y");
    }
    if (is_builtin_zfs_enabled) {
        execute_sed("builtin_zfs", "y");
    }
    if (is_hardly_enabled) {
        execute_sed("hardly", "y");
    }
    if (is_damon_enabled) {
        execute_sed("damon", "y");
    }
    if (is_builtin_zfs_enabled) {
        execute_sed("builtin_zfs", "y");
    }

    // Execute 'sed' with combobox values
    execute_sed("HZ_ticks", get_hz_tick(static_cast<size_t>(m_ui->hztickscomboBox->currentIndex())));
    execute_sed("tickrate", get_tickless_mode(static_cast<size_t>(m_ui->ticklesscomboBox->currentIndex())));

    const std::string_view lto_mode = get_lto_mode(static_cast<size_t>(m_ui->ltocomboBox->currentIndex()));
    if (lto_mode != "no") {
        execute_sed("lto", lto_mode);
    }

    const std::string_view cpu_opt_mode = get_cpu_opt_mode(static_cast<size_t>(m_ui->processor_optcomboBox->currentIndex()));
    if (cpu_opt_mode != "manual") {
        execute_sed("cpu_opt", cpu_opt_mode);
    }

    // Run our build command!
    run_cmd_async("makepkg -sicf --cleanbuild", &m_running);
}
