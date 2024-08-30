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

// NOLINTBEGIN(bugprone-unhandled-exception-at-new)

#include "schedext-window.hpp"
#include "scx_utils.hpp"
#include "utils.hpp"

#include <fstream>
#include <string>
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

#include <QMessageBox>
#include <QProcess>
#include <QStringList>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/compile.h>
#include <fmt/core.h>

namespace {
auto read_kernel_file(std::string_view file_path) noexcept -> std::string {
    std::string file_content{};

    // Skip if failed to open file descriptor.
    std::ifstream file_stream{std::string{file_path}};
    if (!file_stream.is_open()) {
        return {};
    }
    if (!std::getline(file_stream, file_content)) {
        fmt::print(stderr, "Failed to read := '{}'\n", file_path);
        return {};
    }

    return file_content;
}

void spawn_child_process(QString&& cmd, QStringList&& args) noexcept {
    QProcess child_proc;
    child_proc.start(std::move(cmd), std::move(args));
    if (!child_proc.waitForFinished() || child_proc.exitCode() != 0) {
        qWarning() << "child process failed with exit code: " << child_proc.exitCode();
    }
}

auto get_current_scheduler() noexcept -> std::string {
    using namespace std::string_view_literals;

    // NOTE: we assume that window won't be launched on kernel without sched_ext
    // e.g we won't show window at all in that case.
    const auto& current_state = read_kernel_file("/sys/kernel/sched_ext/state"sv);
    if (current_state != "enabled"sv) {
        return current_state;
    }
    const auto& current_sched = read_kernel_file("/sys/kernel/sched_ext/root/ops"sv);
    if (current_sched.empty()) {
        return std::string{"unknown"sv};
    }
    return current_sched;
}

auto is_scx_loader_service_enabled() noexcept -> bool {
    using namespace std::string_view_literals;
    return utils::exec("systemctl is-enabled scx_loader"sv) == "enabled"sv;
}

auto is_scx_loader_service_active() noexcept -> bool {
    using namespace std::string_view_literals;
    return utils::exec("systemctl is-active scx_loader"sv) == "active"sv;
}

auto is_scx_service_enabled() noexcept -> bool {
    using namespace std::string_view_literals;
    return utils::exec("systemctl is-enabled scx"sv) == "enabled"sv;
}

auto is_scx_service_active() noexcept -> bool {
    using namespace std::string_view_literals;
    return utils::exec("systemctl is-active scx"sv) == "active"sv;
}

void disable_scx_loader_service() noexcept {
    if (is_scx_loader_service_enabled()) {
        spawn_child_process("/usr/bin/systemctl", {"disable", "--now", "-f", "scx_loader"});
        fmt::print("Disabling scx_loader service\n");
    } else if (is_scx_loader_service_active()) {
        spawn_child_process("/usr/bin/systemctl", {"stop", "-f", "scx_loader"});
        fmt::print("Stoping scx_loader service\n");
    }
}

void disable_scx_service() noexcept {
    if (is_scx_service_enabled()) {
        spawn_child_process("/usr/bin/systemctl", {"disable", "--now", "-f", "scx"});
        fmt::print("Disabling scx service\n");
    } else if (is_scx_service_active()) {
        spawn_child_process("/usr/bin/systemctl", {"stop", "-f", "scx"});
        fmt::print("Stoping scx service\n");
    }
}

constexpr auto get_scx_mode_from_str(std::string_view scx_mode) noexcept -> scx::SchedMode {
    using namespace std::string_view_literals;

    if (scx_mode == "gaming"sv) {
        return scx::SchedMode::Gaming;
    } else if (scx_mode == "lowlatency"sv) {
        return scx::SchedMode::LowLatency;
    } else if (scx_mode == "powersave"sv) {
        return scx::SchedMode::PowerSave;
    }
    return scx::SchedMode::Auto;
}

}  // namespace

SchedExtWindow::SchedExtWindow(QWidget* parent)
  : QMainWindow(parent), m_sched_timer(new QTimer(this)) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    {
        auto loader_config = scx::loader::Config::init_config(m_config_path);
        if (loader_config.has_value()) {
            m_scx_config = std::make_unique<scx::loader::Config>(std::move(*loader_config));
        } else {
            QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Cannot initialize scx_loader configuration"));
            return;
        }
    }

    // Selecting the scheduler
    auto supported_scheds = scx::loader::get_supported_scheds();
    if (supported_scheds.has_value()) {
        m_ui->schedext_combo_box->addItems(*supported_scheds);
    } else {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Cannot get information from scx_loader!\nIs it working?\nThis is needed for the app to work properly"));

        // hide all components which depends on scheduler management
        m_ui->schedext_combo_box->setHidden(true);
        m_ui->scheduler_select_label->setHidden(true);

        m_ui->schedext_profile_combo_box->setHidden(true);
        m_ui->scheduler_profile_select_label->setHidden(true);

        m_ui->schedext_flags_edit->setHidden(true);
        m_ui->scheduler_set_flags_label->setHidden(true);
    }

    // Selecting the performance profile
    QStringList sched_profiles;
    sched_profiles << "default"
                   << "gaming"
                   << "lowlatency"
                   << "powersave";
    m_ui->schedext_profile_combo_box->addItems(sched_profiles);

    m_ui->current_sched_label->setText(QString::fromStdString(get_current_scheduler()));

    // setup timer
    using namespace std::chrono_literals;  // NOLINT

    connect(m_sched_timer, &QTimer::timeout, this, &SchedExtWindow::update_current_sched);
    m_sched_timer->start(1s);

    connect(m_ui->schedext_combo_box,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &SchedExtWindow::on_sched_changed);
    // Initialize the visibility of the profile selection box
    on_sched_changed();

    // Connect buttons signal
    connect(m_ui->apply_button, &QPushButton::clicked, this, &SchedExtWindow::on_apply);
    connect(m_ui->disable_button, &QPushButton::clicked, this, &SchedExtWindow::on_disable);
}

void SchedExtWindow::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
}

void SchedExtWindow::update_current_sched() noexcept {
    m_ui->current_sched_label->setText(QString::fromStdString(get_current_scheduler()));
}

void SchedExtWindow::on_disable() noexcept {
    m_ui->disable_button->setEnabled(false);
    m_ui->apply_button->setEnabled(false);

    using namespace std::string_view_literals;
    // TODO(vnepogodin): refactor that
    disable_scx_loader_service();

    m_ui->disable_button->setEnabled(true);
    m_ui->apply_button->setEnabled(true);
}

void SchedExtWindow::on_sched_changed() noexcept {
    const auto& scheduler = m_ui->schedext_combo_box->currentText();

    // Show or hide the profile selection UI based on the selected scheduler
    //
    // NOTE: only scx_bpfland and scx_lavd support different preset profiles at
    // the moment.
    if (scheduler == "scx_bpfland" || scheduler == "scx_lavd") {
        m_ui->scheduler_profile_select_label->setVisible(true);
        m_ui->schedext_profile_combo_box->setVisible(true);
    } else {
        m_ui->scheduler_profile_select_label->setVisible(false);
        m_ui->schedext_profile_combo_box->setVisible(false);
    }
}

void SchedExtWindow::on_apply() noexcept {
    m_ui->disable_button->setEnabled(false);
    m_ui->apply_button->setEnabled(false);

    // stop/disable 'scx.service' if its running/enabled on the system,
    // overwise it will conflict
    disable_scx_service();

    // TODO(vnepogodin): refactor that
    const auto& current_selected = m_ui->schedext_combo_box->currentText().toStdString();
    const auto& current_profile  = m_ui->schedext_profile_combo_box->currentText().toStdString();
    const auto& extra_flags      = m_ui->schedext_flags_edit->text().trimmed().toStdString();

    const auto& scx_mode = get_scx_mode_from_str(current_profile);

    auto sched_args = QStringList();
    if (auto scx_flags_for_mode = m_scx_config->scx_flags_for_mode(current_selected, scx_mode); scx_flags_for_mode) {
        if (!scx_flags_for_mode->empty()) {
            sched_args << std::move(*scx_flags_for_mode);
        }
    } else {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Cannot get scx flags from scx_loader configuration!"));
    }

    // NOTE: maybe we should also take into consideration these custom flags,
    // but then the question how it should be displayed/handled
    if (!extra_flags.empty()) {
        sched_args << QString::fromStdString(extra_flags).split(' ');
    }

    fmt::print("Applying scx '{}' with args: {}\n", current_selected, sched_args.join(' ').toStdString());
    auto sched_reply = scx::loader::switch_scheduler_with_args(current_selected, sched_args);
    if (!sched_reply) {
        qDebug() << "Failed to switch '" << current_selected << "' with args:" << sched_args;
    }

    // enable scx_loader service if not enabled yet, it fully replaces scx.service
    if (!is_scx_loader_service_enabled()) {
        fmt::print("Enabling scx_loader service\n");
        spawn_child_process("/usr/bin/systemctl", {"enable", "-f", "scx_loader"});
    }

    // change default scheduler and default scheduler mode
    if (!m_scx_config->set_scx_sched_with_mode(current_selected, scx_mode)) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Cannot set default scx scheduler with mode! Scheduler %1 with mode %2").arg(QString::fromStdString(current_selected), QString::fromStdString(current_profile)));
    }

    // write scx_loader configuration to the temp file
    const auto tmp_config_path = std::string{"/tmp/scx_loader.toml"};
    if (!m_scx_config->write_config_file(tmp_config_path)) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Cannot write scx_loader config to file"));
    }

    // copy scx_loader configuration from the temp file to the actual path with root permissions
    auto config_path = QString::fromStdString(std::string(m_config_path));
    spawn_child_process("/usr/bin/pkexec", {QStringLiteral("/usr/bin/cp"), QString::fromStdString(tmp_config_path), config_path});

    m_ui->disable_button->setEnabled(true);
    m_ui->apply_button->setEnabled(true);
}

// NOLINTEND(bugprone-unhandled-exception-at-new)
