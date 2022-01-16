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

#include "km-window.hpp"
#include "kernel.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

#include <range/v3/algorithm/for_each.hpp>

#pragma clang diagnostic pop
#else
#include <ranges>
namespace ranges = std::ranges;
#endif

#include <future>
#include <iostream>

#include <fmt/core.h>

/*
static inline void stop_process(QProcess* proc) {
    if (proc->state() == QProcess::Running) {
        proc->terminate();
        if (!proc->waitForFinished()) {
            std::cerr << "Process failed to terminate\n";
        }
    }
}*/

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);
    // m_process = std::make_unique<QProcess>(this);

    auto* model = new QStandardItemModel(m_ui->list);
    m_ui->list->setModel(model);

    // In the real code, data is set in each QStandardItem
    model->setColumnCount(2);
    model->setRowCount(static_cast<std::int32_t>(m_kernels.size()));

    std::mutex m;

    // TODO(vnepogodin): parallelize it
    auto a2 = std::async(std::launch::deferred, [&] {
        std::lock_guard<std::mutex> guard(m);
        for (size_t i = 0; i < m_kernels.size(); ++i) {
            const auto& kernel = m_kernels[i];
            auto* item         = new QStandardItem();
            item->setCheckable(true);
            item->setEditable(false);
            item->setText(kernel.get_raw());

            if (kernel.is_installed()) {
                item->setData(Qt::Checked, Qt::CheckStateRole);
            }

            model->setItem(static_cast<std::int32_t>(i), item);
        }
    });

    connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(on_execute()));

    a2.wait();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // stop_process(m_process.get());
    alpm_release(m_handle);

    QWidget::closeEvent(event);
}

void MainWindow::on_cancel() noexcept {
    close();
}

void MainWindow::on_execute() noexcept {
    const auto& model = m_ui->list->model();
    const auto& rows  = model->rowCount();
    for (int i = 0; i < rows; ++i) {
        auto vIndex             = model->index(i, 0);
        const auto& is_selected = model->data(vIndex, Qt::CheckStateRole).toBool();
        if (is_selected) {
            fmt::print(stderr, "i={}\n", i);
        }
    }

    close();
}
