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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#endif

#include <ui_km-window.h>

#include "kernel.hpp"

#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <alpm.h>

#include <QMainWindow>
#include <QThread>
#include <QTimer>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class Work final : public QObject {
    Q_OBJECT

 public:
    using function_t = std::function<void()>;
    explicit Work(function_t func)
      : m_func(func) { }
    virtual ~Work() = default;

 public slots:
    void doHeavyCaclulations();

 private:
    function_t m_func;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)
 public:
    explicit MainWindow(QWidget* parent = nullptr);
    virtual ~MainWindow();

 private slots:
    void on_cancel() noexcept;
    void on_execute() noexcept;

 protected:
    void closeEvent(QCloseEvent* event) override;

 private:
    bool m_running{};
    bool m_thread_running{true};
    int32_t m_last_percent{};
    QString m_last_text{};
    std::mutex m_mutex{};

    QThread* m_worker_th = new QThread(this);
    Work* m_worker{nullptr};

    alpm_errno_t m_err{};
    alpm_handle_t* m_handle              = alpm_initialize("/", "/var/lib/pacman/", &m_err);
    std::vector<Kernel> m_kernels        = Kernel::get_kernels(m_handle);
    std::unique_ptr<Ui::MainWindow> m_ui = std::make_unique<Ui::MainWindow>();

#ifndef PKG_DUMMY_IMPL
    void paintLoop() noexcept;
#endif
};

#endif  // MAINWINDOW_HPP_
