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
#include "compile_options.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdlib>

#include <filesystem>

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

[[gnu::pure]] constexpr const char* get_kernel_name(size_t index) noexcept {
    constexpr std::array kernel_names{"bmq", "bore", "cacule", "cfs", "hardened", "pds", "rc", "tt"};
    return kernel_names[index];
}

[[gnu::pure]] constexpr const char* get_hz_tick(size_t index) noexcept {
    constexpr std::array hz_ticks{"1000", "750", "600", "500", "300", "250", "100"};
    return hz_ticks[index];
}

[[gnu::pure]] constexpr const char* get_tickless_mode(size_t index) noexcept {
    constexpr std::array tickless_modes{"full", "idle", "perodic"};
    return tickless_modes[index];
}

[[gnu::pure]] constexpr const char* get_preempt_mode(size_t index) noexcept {
    constexpr std::array preempt_modes{"full", "voluntary", "server"};
    return preempt_modes[index];
}

[[gnu::pure]] constexpr const char* get_lru_config_mode(size_t index) noexcept {
    constexpr std::array lru_config_modes{"standard", "stats", "none"};
    return lru_config_modes[index];
}

[[gnu::pure]] constexpr const char* get_lto_mode(size_t index) noexcept {
    constexpr std::array lto_modes{"no", "full", "thin"};
    return lto_modes[index];
}

[[gnu::pure]] constexpr const char* get_zstd_comp_level(size_t index) noexcept {
    constexpr std::array zstd_comp_levels{"ultra", "normal"};
    return zstd_comp_levels[index];
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

void execute_sed(std::string_view option, std::string_view value) noexcept {
    const auto& sed_cmd = fmt::format(FMT_COMPILE("sed -i \"s/{}/{}-{}/\" PKGBUILD"), detail::default_option_map.at(option), detail::option_map.at(option), value);
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
    m_ui->main_combo_box->addItems(kernel_names);
    m_ui->main_combo_box->setCurrentIndex(1);

    // Setting default options
    m_ui->cachyconfig_check->setCheckState(Qt::Checked);
    m_ui->hardly_check->setCheckState(Qt::Checked);
    m_ui->perfgovern_check->setCheckState(Qt::Checked);
    m_ui->tcpbbr_check->setCheckState(Qt::Checked);

    QStringList hz_ticks;
    hz_ticks << "1000HZ"
             << "750Hz"
             << "600Hz"
             << "500Hz"
             << "300Hz"
             << "250Hz"
             << "100Hz";
    m_ui->hzticks_combo_box->addItems(hz_ticks);
    m_ui->hzticks_combo_box->setCurrentIndex(3);

    QStringList tickless_modes;
    tickless_modes << "Full"
                   << "Idle"
                   << "Periodic";
    m_ui->tickless_combo_box->addItems(tickless_modes);

    QStringList preempt_modes;
    preempt_modes << "Full"
                  << "Voluntary"
                  << "Server";
    m_ui->preempt_combo_box->addItems(preempt_modes);

    m_ui->mqdio_check->setCheckState(Qt::Checked);
    m_ui->kyber_check->setCheckState(Qt::Checked);

    QStringList lru_config_modes;
    lru_config_modes << "Standard"
                     << "Stats"
                     << "None";
    m_ui->lru_config_combo_box->addItems(lru_config_modes);

    QStringList cpu_optims;
    cpu_optims << "Disabled"
               << "Generic"
               << "Native AMD"
               << "Native Intel"
               << "Zen" << "Zen2" << "Zen3"
               << "Sandy Bridge" << "Ivy Bridge" << "Haswell"
               << "Icelake" << "Tiger Lake" << "Alder Lake";
    m_ui->processor_opt_combo_box->addItems(cpu_optims);

    m_ui->autooptim_check->setCheckState(Qt::Checked);
    m_ui->zstcomp_check->setCheckState(Qt::Checked);
    m_ui->latnice_check->setCheckState(Qt::Checked);

    QStringList zstd_comp_levels;
    zstd_comp_levels << "Ultra"
                     << "Normal";
    m_ui->zstd_comp_levels_combo_box->addItems(zstd_comp_levels);
    m_ui->zstd_comp_levels_combo_box->setCurrentIndex(1);

    QStringList lto_modes;
    lto_modes << "No"
              << "Full"
              << "Thin";
    m_ui->lto_combo_box->addItems(lto_modes);

    // Connect buttons signal
    connect(m_ui->cancel_button, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok_button, SIGNAL(clicked()), this, SLOT(on_execute()));
    connect(m_ui->main_combo_box, &QComboBox::currentIndexChanged, this, [this](std::int32_t index) {
        // Set to 1000HZ, if BMQ, CACULE, PDS, TT
        if (index == 0 || index == 2 || index == 5 || index == 7) {
            m_ui->hzticks_combo_box->setCurrentIndex(0);
        } else {
            m_ui->hzticks_combo_box->setCurrentIndex(3);
        }
        // If not BORE or CFS.
        if (index != 1 && index != 3) {
            m_ui->RT_check->setEnabled(false);
            m_ui->latnice_check->setEnabled(false);
            m_ui->latnice_check->setCheckState(Qt::Unchecked);
            return;
        }
        m_ui->RT_check->setEnabled(true);
        m_ui->latnice_check->setEnabled(true);
        m_ui->latnice_check->setCheckState(Qt::Checked);
    });
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

    const std::int32_t main_combo_index  = m_ui->main_combo_box->currentIndex();
    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(main_combo_index)));
    prepare_build_environment();
    fs::current_path(cpusched_path);

    // Execute 'sed' with checkboxes values
    execute_sed("hardly", convert_checkstate(m_ui->hardly_check));
    execute_sed("per_gov", convert_checkstate(m_ui->perfgovern_check));
    execute_sed("tcp_bbr2", convert_checkstate(m_ui->tcpbbr_check));
    execute_sed("mqdeadline", convert_checkstate(m_ui->mqdio_check));
    execute_sed("kyber", convert_checkstate(m_ui->kyber_check));
    execute_sed("auto_optim", convert_checkstate(m_ui->autooptim_check));
    execute_sed("debug", convert_checkstate(m_ui->debug_check));
    execute_sed("zstd_comp", convert_checkstate(m_ui->zstcomp_check));

    if (main_combo_index == 1 || main_combo_index == 3) {
        execute_sed("rt_kernel", convert_checkstate(m_ui->RT_check));
        execute_sed("latency_nice", convert_checkstate(m_ui->latnice_check));
    }

    const auto& is_cachyconfig_enabled      = (m_ui->cachyconfig_check->checkState() == Qt::Checked);
    const auto& is_nconfig_enabled          = (m_ui->nconfig_check->checkState() == Qt::Checked);
    const auto& is_menuconfig_enabled       = (m_ui->menuconfig_check->checkState() == Qt::Checked);
    const auto& is_xconfig_enabled          = (m_ui->xconfig_check->checkState() == Qt::Checked);
    const auto& is_gconfig_enabled          = (m_ui->gconfig_check->checkState() == Qt::Checked);
    const auto& is_localmodcfg_enabled      = (m_ui->localmodcfg_check->checkState() == Qt::Checked);
    const auto& is_numa_disabled            = (m_ui->numa_check->checkState() == Qt::Checked);
    const auto& is_damon_enabled            = (m_ui->damon_check->checkState() == Qt::Checked);
    const auto& is_lrng_enabled             = (m_ui->lrng_check->checkState() == Qt::Checked);
    const auto& is_builtin_zfs_enabled      = (m_ui->builtin_zfs_check->checkState() == Qt::Checked);
    const auto& is_builtin_bcachefs_enabled = (m_ui->builtin_bcachefs_check->checkState() == Qt::Checked);
    if (!is_cachyconfig_enabled) {
        execute_sed("cachy_config", "'no'");
    }
    if (is_nconfig_enabled) {
        execute_sed("nconfig", "y");
    }
    if (is_menuconfig_enabled) {
        execute_sed("menuconfig", "y");
    }
    if (is_xconfig_enabled) {
        execute_sed("xconfig", "y");
    }
    if (is_gconfig_enabled) {
        execute_sed("gconfig", "y");
    }
    if (is_localmodcfg_enabled) {
        execute_sed("localmodcfg", "y");
    }
    if (is_numa_disabled) {
        execute_sed("numa", "y");
    }
    if (is_damon_enabled) {
        execute_sed("damon", "y");
    }
    if (is_lrng_enabled) {
        execute_sed("lrng", "y");
    }
    if (is_builtin_zfs_enabled) {
        execute_sed("builtin_zfs", "y");
    }
    if (is_builtin_bcachefs_enabled) {
        execute_sed("builtin_bcachefs", "y");
    }

    // Execute 'sed' with combobox values
    execute_sed("HZ_ticks", get_hz_tick(static_cast<size_t>(m_ui->hzticks_combo_box->currentIndex())));
    execute_sed("tickrate", get_tickless_mode(static_cast<size_t>(m_ui->tickless_combo_box->currentIndex())));
    execute_sed("preempt", get_preempt_mode(static_cast<size_t>(m_ui->preempt_combo_box->currentIndex())));
    execute_sed("lru_config", get_lru_config_mode(static_cast<size_t>(m_ui->lru_config_combo_box->currentIndex())));
    execute_sed("zstd_level", get_zstd_comp_level(static_cast<size_t>(m_ui->zstd_comp_levels_combo_box->currentIndex())));

    const std::string_view lto_mode = get_lto_mode(static_cast<size_t>(m_ui->lto_combo_box->currentIndex()));
    if (lto_mode != "no") {
        execute_sed("lto", lto_mode);
    }

    const std::string_view cpu_opt_mode = get_cpu_opt_mode(static_cast<size_t>(m_ui->processor_opt_combo_box->currentIndex()));
    if (cpu_opt_mode != "manual") {
        execute_sed("cpu_opt", cpu_opt_mode);
    }

    // Run our build command!
    run_cmd_async("makepkg -sicf --cleanbuild --skipchecksums", &m_running);
}
