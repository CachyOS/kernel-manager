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

#ifndef MAINWINDOW_HPP_
#define MAINWINDOW_HPP_

#include <ui_km-window.h>

#include "kernel.hpp"

#include <array>
#include <memory>
#include <vector>

#include <QMainWindow>
#include <QProcess>
#include <QStandardItemModel>

class MainWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)
 public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

 private slots:
    void on_cancel() noexcept;
    void on_execute() noexcept;

 protected:
    void closeEvent(QCloseEvent* event) override;

 private:
    std::unique_ptr<QProcess> m_process;
    const std::vector<Kernel> m_kernels  = Kernel::get_kernels();
    std::unique_ptr<Ui::MainWindow> m_ui = std::make_unique<Ui::MainWindow>();
};

#endif  // MAINWINDOW_HPP_
