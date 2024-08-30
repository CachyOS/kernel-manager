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

#ifndef SCHEDEXT_WINDOW_HPP_
#define SCHEDEXT_WINDOW_HPP_

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wsuggest-final-methods"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#include <ui_schedext-window.h>

#include "scx_utils.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QMainWindow>
#include <QTimer>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class SchedExtWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SchedExtWindow)
 public:
    explicit SchedExtWindow(QWidget* parent = nullptr);
    ~SchedExtWindow() = default;

 protected:
    void closeEvent(QCloseEvent* event) override;

 private:
    void on_apply() noexcept;
    void on_disable() noexcept;
    void on_sched_changed() noexcept;

    const std::string_view m_config_path{"/etc/scx_loader.toml"};
    scx::loader::ConfigPtr m_scx_config;
    std::vector<std::string> m_previously_set_options{};
    std::unique_ptr<Ui::SchedExtWindow> m_ui = std::make_unique<Ui::SchedExtWindow>();
    QTimer* m_sched_timer                    = nullptr;

    void update_current_sched() noexcept;
};

#endif  // SCHEDEXT_WINDOW_HPP_
