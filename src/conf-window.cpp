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
#include "config-options.hpp"
#include "utils.hpp"

#include <cstdio>
#include <cstdlib>

#include <algorithm>    // for for_each, transform
#include <filesystem>   // for permissions
#include <ranges>       // for ranges::*
#include <string_view>  // for string_view

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

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
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
    [[gnu::pure]] constexpr ssize_t lookup_##name(std::string_view needle) noexcept { \
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

namespace {

GENERATE_CONST_LOOKUP_OPTION_VALUES(kernel_name, "cachyos", "bore", "rc", "rt", "rt-bore", "sched-ext")
GENERATE_CONST_LOOKUP_OPTION_VALUES(hz_tick, "1000", "750", "600", "500", "300", "250", "100")
GENERATE_CONST_LOOKUP_OPTION_VALUES(tickless_mode, "full", "idle", "perodic")
GENERATE_CONST_LOOKUP_OPTION_VALUES(preempt_mode, "full", "voluntary", "server")
GENERATE_CONST_LOOKUP_OPTION_VALUES(lto_mode, "none", "full", "thin")
GENERATE_CONST_LOOKUP_OPTION_VALUES(hugepage_mode, "always", "madvise")
GENERATE_CONST_LOOKUP_OPTION_VALUES(cpu_opt_mode, "manual", "generic", "native_amd", "native_intel", "zen", "zen2", "zen3", "sandybridge", "ivybridge", "haswell", "icelake", "tigerlake", "alderlake")

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

inline void set_checkstate(QCheckBox* checkbox, bool is_checked) noexcept {
    checkbox->setCheckState(is_checked ? Qt::Checked : Qt::Unchecked);
}

inline auto set_combobox_val(QComboBox* combobox, ssize_t index) noexcept {
    if (index < 0) {
        return 1;
    }
    combobox->setCurrentIndex(static_cast<std::int32_t>(index));
    return 0;
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

auto get_source_array_from_pkgbuild(std::string_view kernel_name_path, std::string_view options_set) noexcept {
    const auto& testscript_src  = fmt::format(FMT_COMPILE("#!/usr/bin/bash\n{}\nsource \"$1\"\n{}"), options_set, "echo \"${source[@]}\"");
    const auto& testscript_path = fmt::format(FMT_COMPILE("{}/.testscript"), kernel_name_path);

    if (utils::write_to_file(testscript_path, testscript_src)) {
        fs::permissions(testscript_path,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    }

    const auto& src_entries = utils::exec(fmt::format(FMT_COMPILE("{} {}/PKGBUILD"), testscript_path, kernel_name_path));
    return utils::make_multiline(src_entries, ' ');
}

auto get_pkgext_value_from_makepkgconf() noexcept -> std::string {
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    static constexpr auto testscript_src = "#!/usr/bin/bash\nsource \"/etc/makepkg.conf\"\necho \"${PKGEXT}\""sv;

    const auto& testscript_path = fmt::format(FMT_COMPILE("{}/.testscriptpkgext"), fs::current_path().string());
    if (utils::write_to_file(testscript_path, testscript_src)) {
        fs::permissions(testscript_path,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    }

    auto pkgext_val = utils::exec(testscript_path);
    if (pkgext_val.empty()) {
        fmt::print(stderr, "failed to get PKGEXT from /etc/makepkg.conf");
        return ".pkg.tar.zst"s;
    }
    return pkgext_val;
}

auto prepare_func_names(std::vector<std::string> parse_lines, std::string_view pkgver_str) noexcept -> std::vector<std::string> {
    using namespace std::string_view_literals;

    static constexpr auto functor = [](auto&& rng) {
        auto rng_str = std::string_view(&*rng.begin(), static_cast<size_t>(std::ranges::distance(rng)));
        return rng_str.starts_with("package_"sv);
    };

    // fetch the pkgext from /etc/makepkg.conf, and fallback to '.pkg.tar.zst' which is default value of makepkg
    const auto& pkgext_val = get_pkgext_value_from_makepkgconf();

    std::vector<std::string> pkg_globs{};
    pkg_globs = parse_lines
        | std::ranges::views::transform([&](auto&& rng) {
              auto&& line = std::string_view(&*rng.begin(), static_cast<size_t>(std::ranges::distance(rng)));

              static constexpr auto needle_prefix = "declare -f "sv;
              if (line.starts_with(needle_prefix)) {
                  line.remove_prefix(needle_prefix.size());
              }
              return line;
          })
        | std::ranges::views::filter(functor)
        | std::ranges::views::transform([&](auto&& rng) {
              auto&& line = std::string_view(&*rng.begin(), static_cast<size_t>(std::ranges::distance(rng)));

              static constexpr auto needle_prefix = "package_"sv;
              if (line.starts_with(needle_prefix)) {
                  line.remove_prefix(needle_prefix.size());
              }
              return fmt::format(FMT_COMPILE("{}-{}-*{}"), line, pkgver_str, pkgext_val);
          })
        | std::ranges::to<std::vector<std::string>>();
    return pkg_globs;
}

auto get_package_names_glob_from_pkgbuild(std::string_view kernel_name_path) noexcept -> std::vector<std::string> {
    using namespace std::string_view_literals;
    static constexpr auto testscript_src = "#!/usr/bin/bash\nsource \"$1\"\ndeclare -F;echo \"pkgver: $pkgver-$pkgrel\""sv;
    static constexpr auto pkgver_prefix  = "pkgver: "sv;

    const auto& testscript_path = fmt::format(FMT_COMPILE("{}/.testscriptpkgnames"), kernel_name_path);
    if (utils::write_to_file(testscript_path, testscript_src)) {
        fs::permissions(testscript_path,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    }

    const auto& src_entries = utils::exec(fmt::format(FMT_COMPILE("{} {}/PKGBUILD"), testscript_path, kernel_name_path));
    const auto& parse_lines = utils::make_multiline(src_entries, '\n');

    auto it = std::ranges::find_if(parse_lines, [](auto&& line) { return line.starts_with(pkgver_prefix); });
    if (it == std::ranges::end(parse_lines)) {
        fmt::print(stderr, "broken pkgbuild; pkgver must be present\n");
        return {};
    }
    auto pkgver_str = std::string_view{*it};
    pkgver_str.remove_prefix(pkgver_prefix.size());

    return prepare_func_names(parse_lines, pkgver_str);
}

bool insert_new_source_array_into_pkgbuild(std::string_view kernel_name_path, QListWidget* list_widget, const std::vector<std::string>& orig_source_array) noexcept {
    static constexpr auto functor = [](auto&& rng) {
        auto rng_str = std::string_view(&*rng.begin(), static_cast<size_t>(std::ranges::distance(rng)));
        return !rng_str.ends_with(".patch");
    };

    auto array_entries = orig_source_array
        | std::ranges::views::filter(functor)
        | std::ranges::views::transform([](auto&& rng) { return fmt::format(FMT_COMPILE("\"{}\""), rng); })
        | std::ranges::to<std::vector<std::string>>();

    // Apply flag to each item in list widget
    for (int i = 0; i < list_widget->count(); ++i) {
        auto* item = list_widget->item(i);
        array_entries.emplace_back(fmt::format(FMT_COMPILE("\"{}\""), item->text().toStdString()));
    }
    const auto& pkgbuild_path = fmt::format(FMT_COMPILE("{}/PKGBUILD"), kernel_name_path);
    auto pkgbuildsrc          = utils::read_whole_file(pkgbuild_path);

    const auto& new_source_array = fmt::format(FMT_COMPILE("source=(\n{})\n"), array_entries | std::ranges::views::join_with('\n') | std::ranges::to<std::string>());
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

}  // namespace

// NOTE: we use std::string const ref intentionally to prevent conversion from string_view into QString
void ConfWindow::run_cmd_async(std::string cmd, const std::string& working_path) noexcept {
    using namespace std::string_literals;
    cmd += "; read -p 'Press enter to exit'"s;

    // remember current build working directory
    m_build_conf_path = working_path;

    m_cmd.setProgram(QStringLiteral("/usr/lib/cachyos-kernel-manager/terminal-helper"));
    m_cmd.setArguments({QString::fromStdString(cmd)});
    m_cmd.setWorkingDirectory(QString::fromStdString(working_path));

    m_cmd.start();

    // connect finish callback
    connect(&m_cmd, &QProcess::finished, this, &ConfWindow::finished_proc, Qt::UniqueConnection);
}

void ConfWindow::finished_proc(int exit_code, QProcess::ExitStatus) noexcept {
    using namespace std::string_view_literals;

    m_running = false;

    // handle exit case
    const auto& check_tmp_path = fmt::format(FMT_COMPILE("{}/.done-status"), m_build_conf_path);
    if (fs::exists(check_tmp_path)) {
        fs::remove(check_tmp_path);

        fmt::print("success\n");

        auto res = QMessageBox::question(this, "CachyOS Kernel Manager", tr("Do you want to install build packages?"));
        if (res == QMessageBox::Yes) {
            fmt::print("pressed yes\n");

            auto pkg_glob_list = get_package_names_glob_from_pkgbuild(m_build_conf_path);
            auto pkg_globs     = pkg_glob_list | std::ranges::views::join_with(' ') | std::ranges::to<std::string>();
            auto pacman_cmd    = fmt::format(FMT_COMPILE("sudo pacman -U {}"), pkg_globs);

            fmt::print("pacman_cmd := {}\n", pacman_cmd);
            m_running = true;
            run_cmd_async(pacman_cmd, m_build_conf_path);
        }
    } else {
        fmt::print(stderr, "process failed with exit code: {}\n", exit_code);
    }
}

void ConfWindow::connect_all_checkboxes() noexcept {
    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    const std::array checkbox_list{
        options_page_ui_obj->builtin_nvidia_check,
        options_page_ui_obj->builtin_nvidia_open_check,
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
    result += convert_to_var_assign_empty_wrapped("builtin_nvidia_open", checkstate_checked(options_page_ui_obj->builtin_nvidia_open_check));
    result += convert_to_var_assign_empty_wrapped("build_debug", checkstate_checked(options_page_ui_obj->build_debug_check));

    // combobox values
    result += convert_to_var_assign("HZ_ticks", get_hz_tick(static_cast<size_t>(options_page_ui_obj->hzticks_combo_box->currentIndex())));
    result += convert_to_var_assign("tickrate", get_tickless_mode(static_cast<size_t>(options_page_ui_obj->tickless_combo_box->currentIndex())));
    result += convert_to_var_assign("preempt", get_preempt_mode(static_cast<size_t>(options_page_ui_obj->preempt_combo_box->currentIndex())));
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
    connect(options_page_ui_obj->save_button, &QPushButton::clicked, this, &ConfWindow::on_save);
    connect(options_page_ui_obj->load_button, &QPushButton::clicked, this, &ConfWindow::on_load);
    connect(options_page_ui_obj->main_combo_box, &QComboBox::currentIndexChanged, this, [this](std::int32_t) {
        reset_patches_data_tab();
    });

    // Setup patches page
    // TODO(vnepogodin): make it lazy loading, only if the user launched the configure window.
    // on window opening setup the page(clone git repo & reset values) run in the background -> show progress bar.
    // prepare_build_environment();
    // reset_patches_data_tab();
    connect_all_checkboxes();

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
        std::ranges::transform(files,
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
    const auto& saved_working_path = fs::current_path().string();
    const auto& build_working_path = fmt::format(FMT_COMPILE("{}/{}"), saved_working_path, cpusched_path);

    // Run our build command!
    run_cmd_async("makepkg -scf --cleanbuild --skipchecksums && touch .done-status", build_working_path);
}

void ConfWindow::on_save() noexcept {
    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    ConfigOptions config_options{};

    // checkboxes values (booleans)
    config_options.hardly_check     = checkstate_checked(options_page_ui_obj->hardly_check);
    config_options.per_gov_check    = checkstate_checked(options_page_ui_obj->perfgovern_check);
    config_options.tcp_bbr3_check   = checkstate_checked(options_page_ui_obj->tcpbbr_check);
    config_options.auto_optim_check = checkstate_checked(options_page_ui_obj->autooptim_check);

    config_options.cachy_config_check        = checkstate_checked(options_page_ui_obj->cachyconfig_check);
    config_options.nconfig_check             = checkstate_checked(options_page_ui_obj->nconfig_check);
    config_options.menuconfig_check          = checkstate_checked(options_page_ui_obj->menuconfig_check);
    config_options.xconfig_check             = checkstate_checked(options_page_ui_obj->xconfig_check);
    config_options.gconfig_check             = checkstate_checked(options_page_ui_obj->gconfig_check);
    config_options.localmodcfg_check         = checkstate_checked(options_page_ui_obj->localmodcfg_check);
    config_options.numa_check                = checkstate_checked(options_page_ui_obj->numa_check);
    config_options.damon_check               = checkstate_checked(options_page_ui_obj->damon_check);
    config_options.builtin_zfs_check         = checkstate_checked(options_page_ui_obj->builtin_zfs_check);
    config_options.builtin_nvidia_check      = checkstate_checked(options_page_ui_obj->builtin_nvidia_check);
    config_options.builtin_nvidia_open_check = checkstate_checked(options_page_ui_obj->builtin_nvidia_open_check);
    config_options.build_debug_check         = checkstate_checked(options_page_ui_obj->build_debug_check);

    // combobox values (strings that we try to find on load)
    config_options.hz_ticks_combo = get_hz_tick(static_cast<size_t>(options_page_ui_obj->hzticks_combo_box->currentIndex()));
    config_options.tickrate_combo = get_tickless_mode(static_cast<size_t>(options_page_ui_obj->tickless_combo_box->currentIndex()));
    config_options.preempt_combo  = get_preempt_mode(static_cast<size_t>(options_page_ui_obj->preempt_combo_box->currentIndex()));
    config_options.hugepage_combo = get_hugepage_mode(static_cast<size_t>(options_page_ui_obj->hugepage_combo_box->currentIndex()));
    config_options.lto_combo      = get_lto_mode(static_cast<size_t>(options_page_ui_obj->lto_combo_box->currentIndex()));
    config_options.cpu_opt_combo  = get_cpu_opt_mode(static_cast<size_t>(options_page_ui_obj->processor_opt_combo_box->currentIndex()));

    config_options.custom_name_edit = options_page_ui_obj->custom_name_edit->text().toStdString();

    auto save_file_path = QFileDialog::getSaveFileName(
        this,
        tr("Save file as"),
        QString::fromStdString(utils::fix_path("~/")),
        tr("Config file (*.toml)"))
                              .toStdString();
    /* clang-format off */
    if (save_file_path.empty()) { return; }
    /* clang-format on */

    if (!ConfigOptions::write_config_file(config_options, save_file_path)) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Failed to save config options to file: %1").arg(QString::fromStdString(save_file_path)));
        return;
    }
}

void ConfWindow::on_load() noexcept {
    auto load_file_path = QFileDialog::getOpenFileName(
        this,
        tr("Load from"),
        QString::fromStdString(utils::fix_path("~/")),
        tr("Config file (*.toml)"))
                              .toStdString();
    /* clang-format off */
    if (load_file_path.empty()) { return; }
    /* clang-format on */

    auto config_options = ConfigOptions::parse_from_file(load_file_path);
    if (!config_options) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Failed to load config options from file: %1").arg(QString::fromStdString(load_file_path)));
        return;
    }

    auto* options_page_ui_obj = m_ui->conf_options_page_widget->get_ui_obj();

    // checkboxes values (booleans)
    set_checkstate(options_page_ui_obj->hardly_check, config_options->hardly_check);
    set_checkstate(options_page_ui_obj->perfgovern_check, config_options->per_gov_check);
    set_checkstate(options_page_ui_obj->tcpbbr_check, config_options->tcp_bbr3_check);
    set_checkstate(options_page_ui_obj->autooptim_check, config_options->auto_optim_check);

    set_checkstate(options_page_ui_obj->cachyconfig_check, config_options->cachy_config_check);
    set_checkstate(options_page_ui_obj->nconfig_check, config_options->nconfig_check);
    set_checkstate(options_page_ui_obj->menuconfig_check, config_options->menuconfig_check);
    set_checkstate(options_page_ui_obj->xconfig_check, config_options->xconfig_check);
    set_checkstate(options_page_ui_obj->gconfig_check, config_options->gconfig_check);
    set_checkstate(options_page_ui_obj->localmodcfg_check, config_options->localmodcfg_check);
    set_checkstate(options_page_ui_obj->numa_check, config_options->numa_check);
    set_checkstate(options_page_ui_obj->damon_check, config_options->damon_check);
    set_checkstate(options_page_ui_obj->builtin_zfs_check, config_options->builtin_zfs_check);
    set_checkstate(options_page_ui_obj->builtin_nvidia_check, config_options->builtin_nvidia_check);
    set_checkstate(options_page_ui_obj->builtin_nvidia_open_check, config_options->builtin_nvidia_open_check);
    set_checkstate(options_page_ui_obj->build_debug_check, config_options->build_debug_check);

    // combobox values (strings that we try to find on load)
    auto combobox_stat = set_combobox_val(options_page_ui_obj->hzticks_combo_box, lookup_hz_tick(config_options->hz_ticks_combo));
    combobox_stat += set_combobox_val(options_page_ui_obj->tickless_combo_box, lookup_tickless_mode(config_options->tickrate_combo));
    combobox_stat += set_combobox_val(options_page_ui_obj->preempt_combo_box, lookup_preempt_mode(config_options->preempt_combo));
    combobox_stat += set_combobox_val(options_page_ui_obj->hugepage_combo_box, lookup_hugepage_mode(config_options->hugepage_combo));
    combobox_stat += set_combobox_val(options_page_ui_obj->lto_combo_box, lookup_lto_mode(config_options->lto_combo));
    combobox_stat += set_combobox_val(options_page_ui_obj->processor_opt_combo_box, lookup_cpu_opt_mode(config_options->cpu_opt_combo));

    options_page_ui_obj->custom_name_edit->setText(QString::fromStdString(config_options->custom_name_edit));

    if (combobox_stat != 0) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Config file(%1) is outdated").arg(QString::fromStdString(load_file_path)));
    }
}
