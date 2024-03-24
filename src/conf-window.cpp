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

#include "conf-window.hpp"
#include "compile_options.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdlib>

#include <filesystem>
#include <string_view>

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

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

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

/**
 * GENERATE_CONST_LOOKUP_VALUES(name, ...):
 *
 * Used to define lookup values of option.
 */
#define GENERATE_CONST_LOOKUP_VALUES(name, ...)                                       \
    [[gnu::pure]] consteval ssize_t lookup_##name(std::string_view needle) noexcept { \
        constexpr std::array list_##name{__VA_ARGS__};                                \
        for (size_t i = 0; i < list_##name.size(); ++i) {                             \
            if (std::string_view{list_##name[i]} == needle) {                         \
                return static_cast<ssize_t>(i);                                       \
            }                                                                         \
        }                                                                             \
        return -1;                                                                    \
    }

/**
 * GENERATE_CONST_LOOKUP_OPTION_VALUES(name, ...):
 *
 * Generates both values lookup and const values functions.
 */
#define GENERATE_CONST_LOOKUP_OPTION_VALUES(name, ...) \
    GENERATE_CONST_OPTION_VALUES(name, __VA_ARGS__)    \
    GENERATE_CONST_LOOKUP_VALUES(name, __VA_ARGS__)

GENERATE_CONST_LOOKUP_OPTION_VALUES(kernel_name, "cachyos", "bore", "rc", "rt", "rt-bore", "sched-ext")
GENERATE_CONST_OPTION_VALUES(hz_tick, "1000", "750", "600", "500", "300", "250", "100")
GENERATE_CONST_OPTION_VALUES(tickless_mode, "full", "idle", "perodic")
GENERATE_CONST_OPTION_VALUES(preempt_mode, "full", "voluntary", "server")
GENERATE_CONST_OPTION_VALUES(lru_config_mode, "standard", "stats", "none")
GENERATE_CONST_OPTION_VALUES(lto_mode, "none", "full", "thin")
GENERATE_CONST_OPTION_VALUES(hugepage_mode, "always", "madvise")
GENERATE_CONST_OPTION_VALUES(cpu_opt_mode, "manual", "generic", "native_amd", "native_intel", "zen", "zen2", "zen3", "sandybridge", "ivybridge", "haswell", "icelake", "tigerlake", "alderlake")

// NOLINTEND(cppcoreguidelines-macro-usage)

static_assert(lookup_kernel_name("cachyos") == 0, "Invalid position");
static_assert(lookup_kernel_name("bore") == 1, "Invalid position");
static_assert(lookup_kernel_name("rc") == 2, "Invalid position");
static_assert(lookup_kernel_name("rt") == 3, "Invalid position");
static_assert(lookup_kernel_name("rt-bore") == 4, "Invalid position");
static_assert(lookup_kernel_name("sched-ext") == 5, "Invalid position");

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
    } else if (kernel_name == "rt"sv) {
        return "linux-cachyos-rt"sv;
    } else if (kernel_name == "tt"sv) {
        return "linux-cachyos-tt"sv;
    } else if (kernel_name == "rt-bore"sv) {
        return "linux-cachyos-rt-bore"sv;
    } else if (kernel_name == "sched-ext"sv) {
        return "linux-cachyos-sched-ext"sv;
    }
    return "linux-cachyos"sv;
}

inline bool checkstate_checked(QCheckBox* checkbox) noexcept {
    return (checkbox->checkState() == Qt::Checked);
}

inline auto convert_checkstate(QCheckBox* checkbox) noexcept {
    using namespace std::string_view_literals;
    return checkstate_checked(checkbox) ? "y"sv : "n"sv;
}

constexpr auto convert_to_varname(std::string_view option) noexcept {
    // force constexpr call with lambda
    return [option] { return detail::option_map.at(option); }();
}

inline auto convert_to_var_assign(std::string_view option, std::string_view value) noexcept {
    return fmt::format(FMT_COMPILE("{}={}\n"), convert_to_varname(option), value);
}

/// return flag to enable if the option is enabled, otherwise do nothing
constexpr auto convert_to_var_assign_empty_wrapped(std::string_view option_name, bool option_enabled) noexcept {
    using namespace std::string_view_literals;
    if (option_enabled) {
        return convert_to_var_assign(option_name, "y"sv);
    }
    return std::string{};
}

void child_watch_cb(GPid pid, [[maybe_unused]] gint status, gpointer user_data) noexcept {
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

void run_cmd_async(std::string cmd, bool* data) noexcept {
    cmd += "; read -p 'Press enter to exit'";
    const gchar* const argv[] = {"/usr/lib/cachyos-kernel-manager/terminal-helper", cmd.c_str(), nullptr};
    gint child_stdout{};
    gint child_stderr{};
    GPid child_pid{};
    g_autoptr(GError) error = nullptr;

    // Spawn child process.
    // NOLINTNEXTLINE
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

bool set_custom_name_in_pkgbuild(std::string_view kernel_name_path, std::string_view custom_name) noexcept {
    const auto& pkgbuild_path = fmt::format(FMT_COMPILE("{}/PKGBUILD"), kernel_name_path);
    auto pkgbuildsrc          = utils::read_whole_file(pkgbuild_path);

    const auto& custom_name_var = fmt::format(FMT_COMPILE("\n\npkgbase=\"{}\""), custom_name);
    if (auto foundpos = pkgbuildsrc.find("_major="); foundpos != std::string::npos) {
        if (auto last_newline_before = pkgbuildsrc.find_last_of('\n', foundpos); last_newline_before != std::string::npos) {
            pkgbuildsrc.insert(last_newline_before, custom_name_var);
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
    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    const std::array checkbox_list{
        options_page_ui_obj->builtin_nvidia_check,
    };

    for (auto* checkbox : checkbox_list) {
        connect(checkbox, &QCheckBox::stateChanged, this, [this](std::int32_t) {
            reset_patches_data_tab();
        });
    }
}

std::string ConfWindow::get_all_set_values() const noexcept {
    std::string result{};
    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    // checkboxes values,
    // which becomes enabled with any value passed,
    // and if nothing passed means it's disabled.
    result += convert_to_var_assign_empty_wrapped("hardly", checkstate_checked(options_page_ui_obj->hardly_check));
    result += convert_to_var_assign_empty_wrapped("per_gov", checkstate_checked(options_page_ui_obj->perfgovern_check));
    result += convert_to_var_assign_empty_wrapped("tcp_bbr3", checkstate_checked(options_page_ui_obj->tcpbbr_check));
    result += convert_to_var_assign_empty_wrapped("auto_optim", checkstate_checked(options_page_ui_obj->autooptim_check));

    result += convert_to_var_assign_empty_wrapped("cachy_config", checkstate_checked(options_page_ui_obj->cachyconfig_check));
    result += convert_to_var_assign_empty_wrapped("nconfig", checkstate_checked(options_page_ui_obj->nconfig_check));
    result += convert_to_var_assign_empty_wrapped("menuconfig", checkstate_checked(options_page_ui_obj->menuconfig_check));
    result += convert_to_var_assign_empty_wrapped("xconfig", checkstate_checked(options_page_ui_obj->xconfig_check));
    result += convert_to_var_assign_empty_wrapped("gconfig", checkstate_checked(options_page_ui_obj->gconfig_check));
    result += convert_to_var_assign_empty_wrapped("localmodcfg", checkstate_checked(options_page_ui_obj->localmodcfg_check));
    result += convert_to_var_assign_empty_wrapped("numa", checkstate_checked(options_page_ui_obj->numa_check));
    result += convert_to_var_assign_empty_wrapped("damon", checkstate_checked(options_page_ui_obj->damon_check));
    result += convert_to_var_assign_empty_wrapped("builtin_zfs", checkstate_checked(options_page_ui_obj->builtin_zfs_check));
    result += convert_to_var_assign_empty_wrapped("builtin_nvidia", checkstate_checked(options_page_ui_obj->builtin_nvidia_check));

    // combobox values
    result += convert_to_var_assign("HZ_ticks", get_hz_tick(static_cast<size_t>(options_page_ui_obj->hzticks_combo_box->currentIndex())));
    result += convert_to_var_assign("tickrate", get_tickless_mode(static_cast<size_t>(options_page_ui_obj->tickless_combo_box->currentIndex())));
    result += convert_to_var_assign("preempt", get_preempt_mode(static_cast<size_t>(options_page_ui_obj->preempt_combo_box->currentIndex())));
    result += convert_to_var_assign("lru_config", get_lru_config_mode(static_cast<size_t>(options_page_ui_obj->lru_config_combo_box->currentIndex())));
    result += convert_to_var_assign("vma_config", get_lru_config_mode(static_cast<size_t>(options_page_ui_obj->vma_config_combo_box->currentIndex())));
    result += convert_to_var_assign("hugepage", get_hugepage_mode(static_cast<size_t>(options_page_ui_obj->hugepage_combo_box->currentIndex())));
    result += convert_to_var_assign("lto", get_lto_mode(static_cast<size_t>(options_page_ui_obj->lto_combo_box->currentIndex())));

    const std::string_view cpu_opt_mode = get_cpu_opt_mode(static_cast<size_t>(options_page_ui_obj->processor_opt_combo_box->currentIndex()));
    if (cpu_opt_mode != "manual") {
        result += convert_to_var_assign("cpu_opt", cpu_opt_mode);
    }

    // NOTE: workaround PKGBUILD incorrectly working with custom pkgname
    const std::string_view lto_mode = get_lto_mode(static_cast<size_t>(options_page_ui_obj->lto_combo_box->currentIndex()));
    if (lto_mode != "none" && options_page_ui_obj->custom_name_edit->text() != "$pkgbase") {
        result += "_use_lto_suffix=n\n";
    }

    return result;
}

void ConfWindow::clear_patches_data_tab() noexcept {
    auto* patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();
    patches_page_ui_obj->list_widget->clear();
}

void ConfWindow::reset_patches_data_tab() noexcept {
    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto* patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    const std::int32_t main_combo_index  = options_page_ui_obj->main_combo_box->currentIndex();
    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(main_combo_index)));

    auto current_array_items = get_source_array_from_pkgbuild(cpusched_path, get_all_set_values());
    std::erase_if(current_array_items, [](auto&& item_el) { return !item_el.ends_with(".patch"); });

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

    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto* patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    // Selecting the CPU scheduler
    QStringList kernel_names;
    kernel_names << tr("CachyOS - BORE + SCHED-EXT")
                 << tr("Bore - Burst-Oriented Response Enhancer")
                 << tr("RC - Release Candidate")
                 << tr("RT - Realtime kernel")
                 << tr("RT-Bore")
                 << tr("Sched-Ext - BPF extensible scheduler class");
    options_page_ui_obj->main_combo_box->addItems(kernel_names);

    // Setting default options
    options_page_ui_obj->cachyconfig_check->setCheckState(Qt::Checked);
    options_page_ui_obj->hardly_check->setCheckState(Qt::Checked);
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
    connect(options_page_ui_obj->cancel_button, &QPushButton::clicked, this, &ConfWindow::on_cancel);
    connect(options_page_ui_obj->ok_button, &QPushButton::clicked, this, &ConfWindow::on_execute);
    connect(options_page_ui_obj->main_combo_box, &QComboBox::currentIndexChanged, this, [this](std::int32_t) {
        reset_patches_data_tab();
    });

    // Setup patches page
    // TODO(vnepogodin): make it lazy loading, only if the user launched the configure window.
    // on window opening setup the page(clone git repo & reset values) run in the background -> show progress bar.
    // prepare_build_environment();
    // reset_patches_data_tab();
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
            [](auto&& file_path) { return QString("file://") + std::forward<decltype(file_path)>(file_path); });

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
        auto* current_item        = patches_page_ui_obj->list_widget->takeItem(current_index);
        patches_page_ui_obj->list_widget->insertItem(current_index - 1, current_item);
        patches_page_ui_obj->list_widget->setCurrentRow(current_index - 1);
    });
    // move down
    connect(patches_page_ui_obj->move_down_button, &QPushButton::clicked, this, [patches_page_ui_obj]() {
        const auto& current_index = patches_page_ui_obj->list_widget->currentRow();
        auto* current_item        = patches_page_ui_obj->list_widget->takeItem(current_index);
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

    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();
    auto* patches_page_ui_obj = m_ui->conf_patches_page_widget->get_ui_obj();

    const std::int32_t main_combo_index  = options_page_ui_obj->main_combo_box->currentIndex();
    const std::string_view cpusched_path = get_kernel_name_path(get_kernel_name(static_cast<size_t>(main_combo_index)));
    utils::prepare_build_environment();

    // Restore clean environment.
    const auto& all_set_values = get_all_set_values();
    utils::restore_clean_environment(m_previously_set_options, all_set_values);

    // Only files which end with .patch,
    // are considered as patches.
    const auto& orig_src_array = get_source_array_from_pkgbuild(cpusched_path, all_set_values);
    auto insert_status         = insert_new_source_array_into_pkgbuild(cpusched_path, patches_page_ui_obj->list_widget, orig_src_array);
    if (!insert_status) {
        m_running = false;
        fmt::print(stderr, "Failed to insert new source array into pkgbuild\n");
        return;
    }
    const auto& custom_name = options_page_ui_obj->custom_name_edit->text().toUtf8();
    insert_status           = set_custom_name_in_pkgbuild(cpusched_path, std::string_view{custom_name.constData(), static_cast<size_t>(custom_name.size())});
    if (!insert_status) {
        m_running = false;
        fmt::print(stderr, "Failed to set custom name in pkgbuild\n");
        return;
    }
    fs::current_path(cpusched_path);

    // Run our build command!
    run_cmd_async("makepkg -sicf --cleanbuild --skipchecksums", &m_running);
}
