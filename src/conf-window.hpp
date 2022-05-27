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

#ifndef CONFWINDOW_HPP_
#define CONFWINDOW_HPP_

#include <ui_conf-window.h>

#include <memory>

#include <QMainWindow>

class ConfWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConfWindow)
 public:
    explicit ConfWindow(QWidget* parent = nullptr);
    virtual ~ConfWindow() = default;

 private slots:
    void on_cancel() noexcept;
    void on_execute() noexcept;

 protected:
    void closeEvent(QCloseEvent* event) override;

 private:
    bool m_running{};
    std::unique_ptr<Ui::ConfWindow> m_ui = std::make_unique<Ui::ConfWindow>();
};

#endif  // CONFWINDOW_HPP_
