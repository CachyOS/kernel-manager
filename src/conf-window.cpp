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
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsuggest-final-types"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <glib.h>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QStringList>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/compile.h>
#include <fmt/core.h>

namespace fs = std::filesystem;

/**
 * GENERATE_CONST_OPTION_VALUES(name, ...):
 *
 * Used to define constant values for options.
 */
#define GENERATE_CONST_OPTION_VALUES(name, ...)                             \
    [[gnu::pure]] constexpr const char* get_##name(size_t index) noexcept { \
        constexpr std::array list_##name{__VA_ARGS__};                      \
        return list_##name[index];                                          \
    }

GENERATE_CONST_OPTION_VALUES(kernel_name, "cachyos", "bmq", "bore", "cfs", "hardened", "pds", "rc", "tt")
GENERATE_CONST_OPTION_VALUES(hz_tick, "1000", "750", "600", "500", "300", "250", "100")
GENERATE_CONST_OPTION_VALUES(tickless_mode, "full", "idle", "perodic")
GENERATE_CONST_OPTION_VALUES(preempt_mode, "full", "voluntary", "server")
GENERATE_CONST_OPTION_VALUES(lru_config_mode, "standard", "stats", "none")
GENERATE_CONST_OPTION_VALUES(lto_mode, "none", "full", "thin")
GENERATE_CONST_OPTION_VALUES(hugepage_mode, "always", "madvise")
GENERATE_CONST_OPTION_VALUES(zstd_comp_level, "ultra", "normal")
GENERATE_CONST_OPTION_VALUES(cpu_opt_mode, "manual", "generic", "native_amd", "native_intel", "zen", "zen2", "zen3", "sandybridge", "ivybridge", "haswell", "icelake", "tigerlake", "alderlake")

constexpr auto get_kernel_name_path(std::string_view kernel_name) noexcept {
    using namespace std::string_view_literals;
    if (kernel_name == "cachyos"sv) {
        return "linux-cachyos"sv;
    } else if (kernel_name == "bmq"sv) {
        return "linux-cachyos-bmq"sv;
    } else if (kernel_name == "bore"sv) {
        return "linux-cachyos-bore"sv;
    } else if (kernel_name == "cfs"sv) {
        return "linux-cachyos-cfs"sv;
    } else if (kernel_name == "hardened"sv) {
        return "linux-cachyos-hardened"sv;
    } else if (kernel_name == "pds"sv) {
        return "linux-cachyos-pds"sv;
    } else if (kernel_name == "rc"sv) {
        return "linux-cachyos-rc"sv;
    } else if (kernel_name == "tt"sv) {
        return "linux-cachyos-tt"sv;
    }
    return "linux-cachyos"sv;
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

inline bool checkstate_checked(QCheckBox* checkbox) noexcept {
    return (checkbox->checkState() == Qt::Checked);
}

inline const char* convert_checkstate(QCheckBox* checkbox) noexcept {
    return checkstate_checked(checkbox) ? "y" : "n";
}

inline constexpr auto convert_to_varname(std::string_view option) noexcept {
    // force constexpr call with lambda
    return [option] { return detail::option_map.at(option); }();
}

inline auto convert_to_var_assign(std::string_view option, std::string_view value) noexcept {
    return fmt::format(FMT_COMPILE("{}={}\n"), convert_to_varname(option), value);
}

inline constexpr auto convert_to_var_assign_empty_wrapped(std::string_view option_name, bool option_enabled) noexcept {
    if (option_enabled) {
        return convert_to_var_assign(option_name, "y");
    }
    return std::string{};
}

void child_watch_cb(GPid pid, [[maybe_unused]] gint status, gpointer user_data) {
#if !defined(NDEBUG)
    fmt::print(stderr, "Child {} exited {}\n", pid,
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
    gint child_stdout{};
    gint child_stderr{};
    GPid child_pid{};
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

auto get_source_array_from_pkgbuild(std::string_view kernel_name_path, std::string_view options_set) noexcept {
    const auto& testscript_src  = fmt::format(FMT_COMPILE("#!/usr/bin/bash\n{}\nsource $1\n{}"), options_set, "echo \"${source[@]}\"");
    const auto& testscript_path = fmt::format(FMT_COMPILE("{}/.testscript"), kernel_name_path);

    if (utils::write_to_file(testscript_path, testscript_src)) {
        fs::permissions(testscript_path,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    }

    const auto& src_entries = utils::exec(fmt::format(FMT_COMPILE("{} {}/PKGBUILD"), testscript_path, kernel_name_path));
    return utils::make_multiline(src_entries, ' ');
}

bool insert_new_source_array_into_pkgbuild(std::string_view kernel_name_path, QListWidget* list_widget, const std::vector<std::string>& orig_source_array) noexcept {
    static constexpr auto functor = [](auto&& rng) {
        auto rng_str = std::string_view(&*rng.begin(), static_cast<size_t>(ranges::distance(rng)));
        return !rng_str.ends_with(".patch");
    };

    std::vector<std::string> array_entries{};
    ranges::for_each(orig_source_array | ranges::views::filter(functor), [&](auto&& rng) { array_entries.emplace_back(fmt::format(FMT_COMPILE("\"{}\""), rng)); });

    // Apply flag to each item in list widget
    for (int i = 0; i < list_widget->count(); ++i) {
        auto* item = list_widget->item(i);
        array_entries.emplace_back(fmt::format(FMT_COMPILE("\"{}\""), item->text().toStdString()));
    }
    const auto& pkgbuild_path = fmt::format(FMT_COMPILE("{}/PKGBUILD"), kernel_name_path);
    auto pkgbuildsrc          = utils::read_whole_file(pkgbuild_path);

    const auto& new_source_array = fmt::format(FMT_COMPILE("source=(\n{})\n"), array_entries | ranges::views::join('\n') | ranges::to<std::string>());
    if (auto foundpos = pkgbuildsrc.find("prepare()"); foundpos != std::string::npos) {
        if (auto last_newline_before = pkgbuildsrc.find_last_of('\n', foundpos); last_newline_before != std::string::npos) {
            pkgbuildsrc.insert(last_newline_before, new_source_array);
        }
    }
    return utils::write_to_file(pkgbuild_path, pkgbuildsrc);
}

auto convert_vector_of_strings_to_stringlist(const std::vector<std::string>& vec) noexcept {
    QStringList result{};

    for (auto&& element : vec) {
        result << QString::fromStdString(element);
    }
    return result;
}

inline void list_widget_apply_edit_flag(QListWidget* list_widget) noexcept {
    // Apply flag to each item in list widget
    for (int i = 0; i < list_widget->count(); ++i) {
        auto* item = list_widget->item(i);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

void ConfWindow::connect_all_checkboxes() noexcept {
    auto options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    std::array checkbox_list{
        options_page_ui_obj->lrng_check,
        options_page_ui_obj->builtin_zfs_check,
        options_page_ui_obj->builtin_bcachefs_check,
    };

    for (auto checkbox : checkbox_list) {
        connect(checkbox, &QCheckBox::stateChanged, this, [this](std::int32_t) {
            reset_patches_data_tab();
        });
    }
}

std::string ConfWindow::get_all_set_values() noexcept {
    std::string result{};
    auto options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    // checkboxes values
    result += convert_to_var_assign("hardly", convert_checkstate(options_page_ui_obj->hardly_check));
    result += convert_to_var_assign("per_gov", convert_checkstate(options_page_ui_obj->perfgovern_check));
    result += convert_to_var_assign("tcp_bbr2", convert_checkstate(options_page_ui_obj->tcpbbr_check));
    result += convert_to_var_assign("mqdeadline", convert_checkstate(options_page_ui_obj->mqdio_check));
    result += convert_to_var_assign("kyber", convert_checkstate(options_page_ui_obj->kyber_check));
    result += convert_to_var_assign("auto_optim", convert_checkstate(options_page_ui_obj->autooptim_check));

    // Execute 'sed' with checkboxes values,
    // which becomes enabled with any value passed,
    // and if nothing passed means it's disabled.
    const auto& is_cachyconfig_enabled = checkstate_checked(options_page_ui_obj->cachyconfig_check);
    if (!is_cachyconfig_enabled) {
        result += convert_to_var_assign("cachy_config", "'no'");
    }
    result += convert_to_var_assign_empty_wrapped("nconfig", checkstate_checked(options_page_ui_obj->nconfig_check));
    result += convert_to_var_assign_empty_wrapped("menuconfig", checkstate_checked(options_page_ui_obj->menuconfig_check));
    result += convert_to_var_assign_empty_wrapped("xconfig", checkstate_checked(options_page_ui_obj->xconfig_check));
    result += convert_to_var_assign_empty_wrapped("gconfig", checkstate_checked(options_page_ui_obj->gconfig_check));
    result += convert_to_var_assign_empty_wrapped("localmodcfg", checkstate_checked(options_page_ui_obj->localmodcfg_check));
    result += convert_to_var_assign_empty_wrapped("numa", checkstate_checked(options_page_ui_obj->numa_check));
    result += convert_to_var_assign_empty_wrapped("damon", checkstate_checked(options_page_ui_obj->damon_check));
    result += convert_to_var_assign_empty_wrapped("lrng", checkstate_checked(options_page_ui_obj->lrng_check));
    result += convert_to_var_assign_empty_wrapped("debug", checkstate_checked(options_page_ui_obj->debug_check));
    result += convert_to_var_assign_empty_wrapped("builtin_zfs", checkstate_checked(options_page_ui_obj->builtin_zfs_check));
    result += convert_to_var_assign_empty_wrapped("builtin_bcachefs", checkstate_checked(options_page_ui_obj->builtin_bcachefs_check));

    // Execute 'sed' with combobox values
    result += convert_to_var_assign("HZ_ticks", get_hz_tick(static_cast<size_t>(options_page_ui_obj->hzticks_combo_box->currentIndex())));
    result += convert_to_var_assign("tickrate", get_tickless_mode(static_cast<size_t>(options_page_ui_obj->tickless_combo_box->currentIndex())));
    result += convert_to_var_assign("preempt", get_preempt_mode(static_cast<size_t>(options_page_ui_obj->preempt_combo_box->currentIndex())));
    result += convert_to_var_assign("lru_config", get_lru_config_mode(static_cast<size_t>(options_page_ui_obj->lru_config_combo_box->currentIndex())));
    result += convert_to_var_assign("vma_config", get_lru_config_mode(static_cast<size_t>(options_page_ui_obj->vma_config_combo_box->currentIndex())));
    result += convert_to_var_assign("zstd_level", get_zstd_comp_level(static_cast<size_t>(options_page_ui_obj->zstd_comp_levels_combo_box->currentIndex())));
    result += convert_to_var_assign("hugepage", get_hugepage_mode(static_cast<size_t>(options_page_ui_obj->hugepage_combo_box->currentIndex())));
    result += convert_to_var_assign("lto", get_lto_mode(static_cast<size_t>(options_page_ui_obj->lto_combo_box->currentIndex())));

    const std::string_view cpu_opt_mode = get_cpu_opt_mode(static_cast<size_t>(options_page_ui_obj->processor_opt_combo_box->currentIndex()));
    if (cpu_opt_mode != "manual") {
        result += convert_to_var_assign("cpu_opt", cpu_opt_mode);
    }

    return result;
}

void ConfWindow::clear_patches_data_tab() noexcept {
    auto patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();
    patches_page_ui_obj->list_widget->clear();
}

void ConfWindow::reset_patches_data_tab() noexcept {
    auto options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    const std::int32_t main_combo_index  = options_page_ui_obj->main_combo_box->currentIndex();
    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(main_combo_index)));

    auto current_array_items = get_source_array_from_pkgbuild(cpusched_path, get_all_set_values());

    current_array_items.erase(std::remove_if(current_array_items.begin(), current_array_items.end(),
                                  [](auto&& item_el) { return !item_el.ends_with(".patch"); }),
        current_array_items.end());
    clear_patches_data_tab();
    patches_page_ui_obj->list_widget->addItems(convert_vector_of_strings_to_stringlist(current_array_items));

    // Apply flag to each item in list widget
    list_widget_apply_edit_flag(patches_page_ui_obj->list_widget);
}

ConfWindow::ConfWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    auto options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    // Selecting the CPU scheduler
    QStringList kernel_names;
    kernel_names << tr("CachyOS - Bore + EEVDF + LATENCY NICE")
                 << tr("BMQ - BitMap Queue CPU scheduler")
                 << tr("Bore - Burst-Oriented Response Enhancer")
                 << tr("CFS - Completely Fair Scheduler")
                 << tr("Hardened - Hardened kernel with the BORE Scheduler")
                 << tr("PDS - Priority and Deadline based Skip list multiple queue CPU scheduler")
                 << tr("RC - Release Candidate")
                 << tr("TT - Task Type Scheduler");
    options_page_ui_obj->main_combo_box->addItems(kernel_names);

    // Setting default options
    options_page_ui_obj->cachyconfig_check->setCheckState(Qt::Checked);
    options_page_ui_obj->hardly_check->setCheckState(Qt::Checked);
    options_page_ui_obj->perfgovern_check->setCheckState(Qt::Checked);
    options_page_ui_obj->tcpbbr_check->setCheckState(Qt::Checked);

    QStringList hz_ticks;
    hz_ticks << "1000HZ"
             << "750Hz"
             << "600Hz"
             << "500Hz"
             << "300Hz"
             << "250Hz"
             << "100Hz";
    options_page_ui_obj->hzticks_combo_box->addItems(hz_ticks);
    options_page_ui_obj->hzticks_combo_box->setCurrentIndex(3);

    QStringList tickless_modes;
    tickless_modes << "Full"
                   << "Idle"
                   << "Periodic";
    options_page_ui_obj->tickless_combo_box->addItems(tickless_modes);

    QStringList preempt_modes;
    preempt_modes << "Full"
                  << "Voluntary"
                  << "Server";
    options_page_ui_obj->preempt_combo_box->addItems(preempt_modes);

    options_page_ui_obj->mqdio_check->setCheckState(Qt::Checked);
    options_page_ui_obj->kyber_check->setCheckState(Qt::Checked);

    QStringList lru_config_modes;
    lru_config_modes << "Standard"
                     << "Stats"
                     << "None";
    options_page_ui_obj->lru_config_combo_box->addItems(lru_config_modes);

    QStringList vma_config_modes;
    vma_config_modes << "Standard"
                     << "Stats"
                     << "None";
    options_page_ui_obj->vma_config_combo_box->addItems(vma_config_modes);
    options_page_ui_obj->vma_config_combo_box->setCurrentIndex(2);

    /* clang-format off */
    QStringList cpu_optims;
    cpu_optims << "Disabled"
               << "Generic"
               << "Native AMD"
               << "Native Intel"
               << "Zen" << "Zen2" << "Zen3"
               << "Sandy Bridge" << "Ivy Bridge" << "Haswell"
               << "Icelake" << "Tiger Lake" << "Alder Lake";
    options_page_ui_obj->processor_opt_combo_box->addItems(cpu_optims);
    /* clang-format on */

    options_page_ui_obj->autooptim_check->setCheckState(Qt::Checked);

    QStringList zstd_comp_levels;
    zstd_comp_levels << "Ultra"
                     << "Normal";
    options_page_ui_obj->zstd_comp_levels_combo_box->addItems(zstd_comp_levels);
    options_page_ui_obj->zstd_comp_levels_combo_box->setCurrentIndex(1);

    QStringList lto_modes;
    lto_modes << "No"
              << "Full"
              << "Thin";
    options_page_ui_obj->lto_combo_box->addItems(lto_modes);

    QStringList hugepage_modes;
    hugepage_modes << "Always"
                   << "Madivse";
    options_page_ui_obj->hugepage_combo_box->addItems(hugepage_modes);

    // Connect buttons signal
    connect(options_page_ui_obj->cancel_button, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(options_page_ui_obj->ok_button, SIGNAL(clicked()), this, SLOT(on_execute()));
    connect(options_page_ui_obj->main_combo_box, &QComboBox::currentIndexChanged, this, [options_page_ui_obj, this](std::int32_t index) {
        // Set to 1000HZ, if BMQ, PDS, TT
        if (index == 1 || index == 5 || index == 7) {
            options_page_ui_obj->hzticks_combo_box->setCurrentIndex(0);
        } else {
            options_page_ui_obj->hzticks_combo_box->setCurrentIndex(3);
        }
        reset_patches_data_tab();
    });

    // Setup patches page
    prepare_build_environment();
    reset_patches_data_tab();
    connect_all_checkboxes();

    connect(options_page_ui_obj->vma_config_combo_box, &QComboBox::currentIndexChanged, this, [this](std::int32_t) {
        reset_patches_data_tab();
    });

    // local patches
    connect(patches_page_ui_obj->local_patch_button, &QPushButton::clicked, this, [this, patches_page_ui_obj] {
        auto files = QFileDialog::getOpenFileNames(
            this,
            tr("Select one or more patch files"),
            QString::fromStdString(utils::fix_path("~/")),
            tr("Patch file (*.patch)"));
        /* clang-format off */
        if (files.isEmpty()) { return; }
        /* clang-format on */

        // Prepend 'file://' to each selected patch file.
        std::transform(files.cbegin(), files.cend(),
            files.begin(),  // write to the same location
            [](auto&& file_path) { return QString("file://") + std::move(file_path); });

        patches_page_ui_obj->list_widget->addItems(files);

        // Apply flag to each item in list widget
        list_widget_apply_edit_flag(patches_page_ui_obj->list_widget);
    });
    // remote patches
    connect(patches_page_ui_obj->remote_patch_button, &QPushButton::clicked, this, [this, patches_page_ui_obj] {
        bool is_confirmed{};
        const auto& patch_url_text = QInputDialog::getText(
            this,
            tr("Enter URL patch"),
            tr("Patch URL:"), QLineEdit::Normal,
            QString(), &is_confirmed);
        /* clang-format off */
        if (!is_confirmed || patch_url_text.isEmpty()) { return; }
        /* clang-format on */

        patches_page_ui_obj->list_widget->addItems(QStringList() << patch_url_text);

        // Apply flag to each item in list widget
        list_widget_apply_edit_flag(patches_page_ui_obj->list_widget);
    });

    patches_page_ui_obj->remove_entry_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    patches_page_ui_obj->move_up_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowUp));
    patches_page_ui_obj->move_down_button->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowDown));

    // remove entry
    connect(patches_page_ui_obj->remove_entry_button, &QPushButton::clicked, this, [patches_page_ui_obj]() {
        const auto& current_index = patches_page_ui_obj->list_widget->currentRow();
        delete patches_page_ui_obj->list_widget->takeItem(current_index);
    });

    // move up
    connect(patches_page_ui_obj->move_up_button, &QPushButton::clicked, this, [patches_page_ui_obj]() {
        const auto& current_index = patches_page_ui_obj->list_widget->currentRow();
        auto current_item         = patches_page_ui_obj->list_widget->takeItem(current_index);
        patches_page_ui_obj->list_widget->insertItem(current_index - 1, current_item);
        patches_page_ui_obj->list_widget->setCurrentRow(current_index - 1);
    });
    // move down
    connect(patches_page_ui_obj->move_down_button, &QPushButton::clicked, this, [patches_page_ui_obj]() {
        const auto& current_index = patches_page_ui_obj->list_widget->currentRow();
        auto current_item         = patches_page_ui_obj->list_widget->takeItem(current_index);
        patches_page_ui_obj->list_widget->insertItem(current_index + 1, current_item);
        patches_page_ui_obj->list_widget->setCurrentRow(current_index + 1);
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
    /* clang-format off */
    if (m_running) { return; }
    /* clang-format on */
    m_running = true;

    auto options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    const std::int32_t main_combo_index  = options_page_ui_obj->main_combo_box->currentIndex();
    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(main_combo_index)));
    prepare_build_environment();

    const auto& all_set_values  = get_all_set_values();
    const auto& set_values_list = utils::make_multiline(all_set_values, '\n');
    for (const auto& expr : set_values_list) {
        const auto& expr_split = utils::make_multiline(expr, '=');
        const auto& var_name   = expr_split[0];
        const auto& var_val    = expr_split[1];

        if (setenv(var_name.c_str(), var_val.c_str(), 1) != 0) {
            fmt::print(stderr, "Cannot set environment variable!");
        }
    }

    // Only files which end with .patch,
    // are considered as patches.
    const auto& orig_src_array                 = get_source_array_from_pkgbuild(cpusched_path, all_set_values);
    [[maybe_unused]] const auto& insert_status = insert_new_source_array_into_pkgbuild(cpusched_path, patches_page_ui_obj->list_widget, orig_src_array);
    fs::current_path(cpusched_path);

    // Run our build command!
    run_cmd_async("makepkg -sicf --cleanbuild --skipchecksums", &m_running);
}
