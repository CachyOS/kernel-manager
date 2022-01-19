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

#include <fmt/color.h>
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

QProgressBar* g_progress_bar;
QLabel* g_label;

/* callback to handle display of progress */
void cb_progress(void* ctx, alpm_progress_t event, const char* pkgname,
    int percent, size_t howmany, size_t remain);

/* callback to handle messages/notifications from pacman library */
__attribute__((format(printf, 3, 0))) void cb_log(void* ctx, alpm_loglevel_t level, const char* fmt, va_list args);

void cb_progress(void* ctx, alpm_progress_t event, const char* pkgname,
    int percent, size_t howmany, size_t remain) {
    (void)ctx;
    std::string opr{};
    /* set text of message to display */
    switch (event) {
    case ALPM_PROGRESS_ADD_START:
        opr = "installing";
        break;
    case ALPM_PROGRESS_UPGRADE_START:
        opr = "upgrading";
        break;
    case ALPM_PROGRESS_DOWNGRADE_START:
        opr = "downgrading";
        break;
    case ALPM_PROGRESS_REINSTALL_START:
        opr = "reinstalling";
        break;
    case ALPM_PROGRESS_REMOVE_START:
        opr = "removing";
        break;
    case ALPM_PROGRESS_CONFLICTS_START:
        opr = "checking for file conflicts";
        break;
    case ALPM_PROGRESS_DISKSPACE_START:
        opr = "checking available disk space";
        break;
    case ALPM_PROGRESS_INTEGRITY_START:
        opr = "checking package integrity";
        break;
    case ALPM_PROGRESS_KEYRING_START:
        opr = "checking keys in keyring";
        break;
    case ALPM_PROGRESS_LOAD_START:
        opr = "loading package files";
        break;
    default:
        return;
    }

    g_progress_bar->setValue(percent);
    g_label->setText(opr.c_str());
    if (percent == 100) {
        fmt::print("pkg_name={}, status=done\n", pkgname);
    } else {
        fmt::print("pkg_name={}, percent={}, remains={}, howmany={}\n", pkgname, percent, remain, howmany);
    }
}

void cb_log(void* ctx, alpm_loglevel_t level, const char* fmt, va_list args) {
    (void)ctx;
    if (!fmt || strlen(fmt) == 0) {
        return;
    }

#ifndef ALPM_DEBUG
    if (level == ALPM_LOG_DEBUG) {
        return;
    }
#else
    (void)level;
#endif

    std::vfprintf(stderr, fmt, args);
}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);
    // m_process = std::make_unique<QProcess>(this);

    g_progress_bar = m_ui->progressBar;
    g_label        = m_ui->progress_status;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
    alpm_option_set_logcb(m_handle, cb_log, NULL);
    alpm_option_set_progresscb(m_handle, cb_progress, NULL);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    //    alpm_option_set_dlcb(m_handle, cb_download, NULL);
    //    alpm_option_set_eventcb(m_handle, cb_event, NULL);
    //    alpm_option_set_questioncb(m_handle, cb_question, NULL);

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

static void install_packages(alpm_handle_t* handle, const std::vector<Kernel>& kernels, const QAbstractItemModel* model) {
    /* Step 0: create a new transaction */
    if (alpm_trans_init(handle, ALPM_TRANS_FLAG_ALLDEPS | ALPM_TRANS_FLAG_ALLEXPLICIT) == -1) {
        fmt::print(stderr, "failed to create a new transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_trans_release(handle);
        return;
    }

    /* Step 1: add targets to the created transaction */
    const bool is_root = utils::check_root();
    const auto& rows   = model->rowCount();
    for (int i = 0; i < rows; ++i) {
        const auto& kernel      = kernels[static_cast<size_t>(i)];
        auto vIndex             = model->index(i, 0);
        const auto& is_selected = model->data(vIndex, Qt::CheckStateRole).toBool();
        if (is_root && is_selected && !kernel.is_installed()) {
            if (!kernel.install()) {
                fmt::print(stderr, "failed to add package to be installed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

    /* Step 2: prepare the transaction based on its type, targets and flags */
    alpm_list_t* trans_data = nullptr;
    if (alpm_trans_prepare(handle, &trans_data) == -1) {
        auto err = alpm_errno(handle);
        fmt::print(stderr, "failed to prepare transaction ({})\n", alpm_strerror(err));
        if (err == ALPM_ERR_UNSATISFIED_DEPS) {
            for (alpm_list_t* i = trans_data; i; i = alpm_list_next(i)) {
                auto* miss = reinterpret_cast<alpm_depmissing_t*>(i->data);
                const std::unique_ptr<char> depstring{alpm_dep_compute_string(miss->depend)};
                fmt::print(stderr, "removing {} breaks dependency '{}' required by {}\n", miss->causingpkg, depstring.get(), miss->target);
                alpm_depmissing_free(miss);
            }
        }
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 3: actually perform the installation */
    const auto* inst_packages = alpm_trans_get_add(handle);
    if (inst_packages == nullptr) {
        fmt::print(stderr, "there is nothing to do\n");
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    } else {
        print_packages(inst_packages);
    }

    if (alpm_trans_commit(handle, &trans_data) == -1) {
        fmt::print(stderr, "failed to commit transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
}

static void remove_packages(alpm_handle_t* handle, const std::vector<Kernel>& kernels, const QAbstractItemModel* model) {
    /* Step 0: create a new transaction */
    if (alpm_trans_init(handle, ALPM_TRANS_FLAG_ALLDEPS) == -1) {
        fmt::print(stderr, "failed to create a new transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_trans_release(handle);
        return;
    }

    /* Step 1: add targets to the created transaction */
    const bool is_root = utils::check_root();
    const auto& rows   = model->rowCount();
    for (int i = 0; i < rows; ++i) {
        const auto& kernel      = kernels[static_cast<size_t>(i)];
        auto vIndex             = model->index(i, 0);
        const auto& is_selected = model->data(vIndex, Qt::CheckStateRole).toBool();
        if (is_root && !is_selected && kernel.is_installed()) {
            if (!kernel.remove()) {
                fmt::print(stderr, "failed to add package to be removed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

    /* Step 2: prepare the transaction based on its type, targets and flags */
    alpm_list_t* trans_data = nullptr;
    if (alpm_trans_prepare(handle, &trans_data) == -1) {
        auto err = alpm_errno(handle);
        fmt::print(stderr, "failed to prepare transaction ({})\n", alpm_strerror(err));
        if (err == ALPM_ERR_UNSATISFIED_DEPS) {
            for (alpm_list_t* i = trans_data; i; i = alpm_list_next(i)) {
                auto* miss = reinterpret_cast<alpm_depmissing_t*>(i->data);
                const std::unique_ptr<char> depstring{alpm_dep_compute_string(miss->depend)};
                fmt::print(stderr, "removing {} breaks dependency '{}' required by {}\n", miss->causingpkg, depstring.get(), miss->target);
                alpm_depmissing_free(miss);
            }
        }
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 3: actually perform the removal */
    const auto* remove_packages = alpm_trans_get_remove(handle);
    if (remove_packages == nullptr) {
        fmt::print(stderr, "there is nothing to do\n");
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    } else {
        print_packages(remove_packages);
    }

    if (alpm_trans_commit(handle, &trans_data) == -1) {
        fmt::print(stderr, "failed to commit transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
}

void MainWindow::on_execute() noexcept {
    install_packages(m_handle, m_kernels, m_ui->list->model());
    remove_packages(m_handle, m_kernels, m_ui->list->model());

    // close();
}
