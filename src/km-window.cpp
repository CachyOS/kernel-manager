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
#include "utils.hpp"

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

    auto* model = new QStandardItemModel(0, 3, nullptr);
    // model->setHeaderData(0, Qt::Horizontal, QObject::tr("Choose"));
    // model->setHeaderData(1, Qt::Horizontal, QObject::tr("asdsa"));

    model->setHorizontalHeaderLabels(QStringList() << "Code"
                                                   << "Definition");

    // m_ui->list->setModelColumn(3);
    m_ui->list->setModel(model);
    // model->columnCount(3);

    // In the real code, data is set in each QStandardItem
    model->setColumnCount(3);
    model->setRowCount(static_cast<std::int32_t>(m_kernels.size()));

    QItemSelectionModel* selection = new QItemSelectionModel(model);
    m_ui->list->setSelectionModel(selection);

    m_ui->list->activateWindow();

    std::mutex m;

    // TODO(vnepogodin): parallelize it
    auto a2 = std::async(std::launch::deferred, [&] {
        std::lock_guard<std::mutex> guard(m);
        for (size_t i = 0; i < m_kernels.size(); ++i) {
            const auto& kernel = m_kernels[i];
            auto* item         = new QStandardItem();
            item->setCheckable(true);
            item->setEditable(false);
            item->setText(fmt::format("{} \t{}", kernel.get_raw(), kernel.version()).c_str());

            auto* second = new QStandardItem();
            second->setCheckable(true);
            second->setText(kernel.get_raw());

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

static void print_packages(const alpm_list_t* packages) {
    const alpm_list_t* i;
    for (i = packages; i; i = alpm_list_next(i)) {
        auto* pkg = reinterpret_cast<alpm_pkg_t*>(i->data);

        fmt::print(stderr, "pkgname={}\n", alpm_pkg_get_name(pkg));
    }
}

void MainWindow::on_execute() noexcept {
    /* Step 0: create a new transaction */
    alpm_trans_init(m_handle, ALPM_TRANS_FLAG_ALLDEPS);

    /* Step 1: add targets to the created transaction */
    const bool is_root = utils::check_root();
    const auto& model  = m_ui->list->model();
    const auto& rows   = model->rowCount();
    for (int i = 0; i < rows; ++i) {
        const auto& kernel      = m_kernels[static_cast<size_t>(i)];
        auto vIndex             = model->index(i, 0);
        const auto& is_selected = model->data(vIndex, Qt::CheckStateRole).toBool();
        if (is_root && is_selected && !kernel.is_installed()) {
            kernel.install();
        } else if (is_root && !is_selected && kernel.is_installed()) {
            kernel.remove();
        }
    }

    /* Step 2: prepare the transaction based on its type, targets and flags */
    alpm_list_t* trans_data = nullptr;
    if (alpm_trans_prepare(m_handle, &trans_data) == -1) {
        alpm_list_free(trans_data);
        alpm_trans_release(m_handle);
        return;
    }

    /* Step 3: actually perform the removal */
    const auto* inst_packages = alpm_trans_get_add(m_handle);
    if (inst_packages == nullptr) {
        fmt::print(stderr, "there is nothing to do\n");
    } else {
        print_packages(inst_packages);
    }

    //    const auto* remove_packages = alpm_trans_get_remove(m_handle);
    //    if (remove_packages == nullptr) {
    //        fmt::print(stderr, "there is nothing to do\n");
    //    } else {
    //        print_packages(remove_packages);
    //    }

    if (alpm_trans_commit(m_handle, &trans_data) == -1) {
        fmt::print(stderr, "failed to commit transaction ({})\n", alpm_strerror(alpm_errno(m_handle)));
        alpm_list_free(trans_data);
        alpm_trans_release(m_handle);
        return;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(m_handle);
}
