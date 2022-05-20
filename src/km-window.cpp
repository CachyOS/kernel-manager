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

#include <future>
#include <thread>

#include <fmt/core.h>

#include <QCoreApplication>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QTreeWidgetItem>

static int32_t* g_last_percent{};
static QString* g_last_text{};

/* callback to handle messages/notifications from libalpm */
void cb_event(void* ctx, alpm_event_t* event);

/* callback to handle display of progress */
void cb_progress(void* ctx, alpm_progress_t event, const char* pkgname,
    int percent, size_t howmany, size_t remain);

/* callback to handle display of download progress */
void cb_download(void* ctx, const char* filename, alpm_download_event_type_t event,
    void* data);

/* callback to handle messages/notifications from pacman library */
__attribute__((format(printf, 3, 0))) void cb_log(void* ctx, alpm_loglevel_t level, const char* fmt, va_list args);

void cb_event(void* ctx, alpm_event_t* event) {
    (void)ctx;

    std::string opr{"processing..."};
    switch (event->type) {
    case ALPM_EVENT_HOOK_START:
        if (event->hook.when == ALPM_HOOK_PRE_TRANSACTION) {
            opr = "Running pre-transaction hooks...";
            break;
        }
        opr = "Running post-transaction hooks...";
        break;
    case ALPM_EVENT_CHECKDEPS_START:
        opr = "checking dependencies...";
        break;
    case ALPM_EVENT_FILECONFLICTS_START:
        opr = "checking for file conflicts...";
        break;
    case ALPM_EVENT_RESOLVEDEPS_START:
        opr = "resolving dependencies...";
        break;
    case ALPM_EVENT_INTERCONFLICTS_START:
        opr = "looking for conflicting packages...";
        break;
    case ALPM_EVENT_TRANSACTION_START:
        opr = "Processing package changes...";
        break;
    case ALPM_EVENT_PACKAGE_OPERATION_START: {
        alpm_event_package_operation_t* e = &event->package_operation;
        auto* safe_pkg                    = (e->newpkg == nullptr) ? e->oldpkg : e->newpkg;

        switch (e->operation) {
        case ALPM_PACKAGE_INSTALL:
            opr = fmt::format("installing {}...", alpm_pkg_get_name(safe_pkg));
            break;
        case ALPM_PACKAGE_UPGRADE:
            opr = fmt::format("upgrading {}...", alpm_pkg_get_name(safe_pkg));
            break;
        case ALPM_PACKAGE_REINSTALL:
            opr = fmt::format("reinstalling {}...", alpm_pkg_get_name(safe_pkg));
            break;
        case ALPM_PACKAGE_DOWNGRADE:
            opr = fmt::format("downgrading {}...", alpm_pkg_get_name(safe_pkg));
            break;
        case ALPM_PACKAGE_REMOVE:
            opr = fmt::format("removing {}...", alpm_pkg_get_name(safe_pkg));
            break;
        }
    } break;
    case ALPM_EVENT_INTEGRITY_START:
        opr = "checking package integrity...";
        break;
    case ALPM_EVENT_KEYRING_START:
        opr = "checking keyring...";
        break;
    case ALPM_EVENT_KEY_DOWNLOAD_START:
        opr = "downloading required keys...";
        break;
    case ALPM_EVENT_LOAD_START:
        opr = "loading package files...";
        break;
    case ALPM_EVENT_SCRIPTLET_INFO:
        fputs(event->scriptlet_info.line, stdout);
        break;
    case ALPM_EVENT_PKG_RETRIEVE_START:
        opr = "Retrieving packages...";
        break;
    case ALPM_EVENT_DISKSPACE_START:
        opr = "checking available disk space...";
        break;
    case ALPM_EVENT_PKG_RETRIEVE_DONE:
        opr = "Package retrieve done.";
        break;
    case ALPM_EVENT_DATABASE_MISSING:
        opr = "DB is missing.";
        break;
    case ALPM_EVENT_PACKAGE_OPERATION_DONE:
        opr = "Package operation done.";
        break;
    case ALPM_EVENT_HOOK_RUN_START:
        opr             = "Starting hook run...";
        *g_last_percent = 0;
        break;
    case ALPM_EVENT_HOOK_RUN_DONE:
        opr             = "Done hook run.";
        *g_last_percent = 100;
        break;
    case ALPM_EVENT_HOOK_DONE:
        opr             = "Hook is done.";
        *g_last_percent = 100;
        break;
    // all the simple done events, with fallthrough for each
    case ALPM_EVENT_OPTDEP_REMOVAL:
    case ALPM_EVENT_DB_RETRIEVE_START:
    case ALPM_EVENT_PACNEW_CREATED:
    case ALPM_EVENT_PACSAVE_CREATED:
    case ALPM_EVENT_DB_RETRIEVE_DONE:
    case ALPM_EVENT_DB_RETRIEVE_FAILED:
    case ALPM_EVENT_PKG_RETRIEVE_FAILED:
    case ALPM_EVENT_FILECONFLICTS_DONE:
    case ALPM_EVENT_CHECKDEPS_DONE:
    case ALPM_EVENT_RESOLVEDEPS_DONE:
    case ALPM_EVENT_INTERCONFLICTS_DONE:
    case ALPM_EVENT_TRANSACTION_DONE:
    case ALPM_EVENT_INTEGRITY_DONE:
    case ALPM_EVENT_KEYRING_DONE:
    case ALPM_EVENT_KEY_DOWNLOAD_DONE:
    case ALPM_EVENT_LOAD_DONE:
    case ALPM_EVENT_DISKSPACE_DONE:
        /* nothing */
        return;
    }

    *g_last_text = QString(opr.c_str());
}

void cb_progress(void* ctx, alpm_progress_t event, const char* pkgname, int percent, size_t howmany, size_t remain) {
    (void)ctx;
    std::string opr{"processing"};
    // set text of message to display
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

    if (percent == 100) {
        *g_last_text = QString(fmt::format("({}/{}) {} done", remain, howmany, pkgname).c_str());
        return;
    }
    *g_last_text = QString(fmt::format("({}/{}) {}", remain, howmany, opr).c_str());
}

static char* clean_filename(const char* filename) {
    size_t len = strlen(filename);
    char* p;
    char* fname = new char[len + 1];
    memcpy(fname, filename, len + 1);
    // strip package or DB extension for cleaner look
    if ((p = strstr(fname, ".pkg")) || (p = strstr(fname, ".db")) || (p = strstr(fname, ".files"))) {
        len        = static_cast<size_t>(p - fname);
        fname[len] = '\0';
    }

    return fname;
}

/* Handles download progress event */
static void dload_progress_event(const char* filename, alpm_download_event_progress_t* data) {
    (void)filename;

    const auto& percent = (data->downloaded * 100) / data->total;
    *g_last_percent     = static_cast<int32_t>(percent);
}

/* download completed */
static void dload_complete_event(const char* filename, alpm_download_event_completed_t* data) {
    std::unique_ptr<char[]> cleaned_filename{clean_filename(filename)};

    if (data->result == 1) {
        // The line contains text from previous status. Erase these leftovers.
        *g_last_text    = fmt::format("{} is up to date", cleaned_filename.get()).c_str();
        *g_last_percent = 0;
    } else if (data->result == 0) {
        *g_last_text = fmt::format("{} download completed successfully", cleaned_filename.get()).c_str();
    } else {
        *g_last_text = fmt::format("{} failed to download", cleaned_filename.get()).c_str();
    }
}

void cb_download(void* ctx, const char* filename, alpm_download_event_type_t event, void* data) {
    (void)ctx;

    // do not print signature files progress bar
    const std::string file_name_{filename};
    if (file_name_.ends_with(".sig")) {
        return;
    }

    if (event == ALPM_DOWNLOAD_INIT) {
        std::unique_ptr<char[]> cleaned_filename{clean_filename(filename)};
        *g_last_percent = 0;
        *g_last_text    = fmt::format("{} downloading...", cleaned_filename.get()).c_str();
    } else if (event == ALPM_DOWNLOAD_PROGRESS) {
        dload_progress_event(filename, reinterpret_cast<alpm_download_event_progress_t*>(data));
    } else if (event == ALPM_DOWNLOAD_RETRY) {
        *g_last_percent = 0;
        return;
    } else if (event == ALPM_DOWNLOAD_COMPLETED) {
        dload_complete_event(filename, reinterpret_cast<alpm_download_event_completed_t*>(data));
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

    if (level == ALPM_LOG_ERROR || level == ALPM_LOG_WARNING) {
        std::vfprintf(stderr, fmt, args);
    }
}

void install_packages(alpm_handle_t* handle, const std::vector<Kernel>& kernels, const std::vector<std::string>& selected_list) {
#ifdef PKG_DUMMY_IMPL
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if ((kernel != kernels.end()) && (!kernel->is_installed() || kernel->is_update_available())) {
            if (!kernel->install()) {
                fmt::print(stderr, "failed to add package to be installed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }
#else
    /* Step 0: create a new transaction */
    if (alpm_trans_init(handle, ALPM_TRANS_FLAG_ALLDEPS | ALPM_TRANS_FLAG_ALLEXPLICIT) != 0) {
        fmt::print(stderr, "failed to create a new transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_trans_release(handle);
        return;
    }

    /* Step 1: add targets to the created transaction */
    const bool is_root = utils::check_root();
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if (is_root && (kernel != kernels.end()) && (!kernel->is_installed() || kernel->is_update_available())) {
            if (!kernel->install()) {
                fmt::print(stderr, "failed to add package to be installed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

    /* Step 2: prepare the transaction based on its type, targets and flags */
    alpm_list_t* trans_data = nullptr;
    if (alpm_trans_prepare(handle, &trans_data) != 0) {
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
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    if (alpm_trans_commit(handle, &trans_data) != 0) {
        fmt::print(stderr, "failed to commit transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
#endif
}

void remove_packages(alpm_handle_t* handle, const std::vector<Kernel>& kernels, const std::vector<std::string>& selected_list) {
#ifdef PKG_DUMMY_IMPL
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if ((kernel != kernels.end()) && (kernel->is_installed())) {
            if (!kernel->remove()) {
                fmt::print(stderr, "failed to add package to be removed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

#else
    /* Step 0: create a new transaction */
    if (alpm_trans_init(handle, ALPM_TRANS_FLAG_ALLDEPS) != 0) {
        fmt::print(stderr, "failed to create a new transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_trans_release(handle);
        return;
    }

    /* Step 1: add targets to the created transaction */
    const bool is_root = utils::check_root();
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if (is_root && (kernel != kernels.end()) && (kernel->is_installed())) {
            if (!kernel->remove()) {
                fmt::print(stderr, "failed to add package to be removed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

    /* Step 2: prepare the transaction based on its type, targets and flags */
    alpm_list_t* trans_data = nullptr;
    if (alpm_trans_prepare(handle, &trans_data) != 0) {
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
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    if (alpm_trans_commit(handle, &trans_data) != 0) {
        fmt::print(stderr, "failed to commit transaction ({})\n", alpm_strerror(alpm_errno(handle)));
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
#endif
}

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    // Setup global values
    g_last_text    = &m_last_text;
    g_last_percent = &m_last_percent;

#ifdef PKG_DUMMY_IMPL
    m_ui->progress_status->hide();
    m_ui->progressBar->hide();
#endif

    // Create worker thread
    m_worker = new Work([&]() {
        while (m_thread_running) {
            auto smth = fmt::format("...");
            if (m_running) {
                m_ui->ok->setEnabled(false);

                std::vector<std::string> change_list(size_t(m_change_list.size()));
                for (size_t i = 0; i < change_list.size(); ++i) {
                    change_list[i] = m_change_list[int(i)].toStdString();
                }

                install_packages(m_handle, m_kernels, change_list);
                remove_packages(m_handle, m_kernels, change_list);

#ifdef PKG_DUMMY_IMPL
                Kernel::commit_transaction();
#endif
                m_last_percent = 100;
                m_last_text    = "Done";

                m_running = false;
                m_ui->ok->setEnabled(true);
            }
        }
    });

    m_worker->moveToThread(m_worker_th);
    // name to appear in ps, task manager, etc.
    m_worker_th->setObjectName("WorkerThread");

#ifndef PKG_DUMMY_IMPL
    static constexpr auto TIMEOUT_MS = 50;

    // Set update timer
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, QOverload<>::of(&MainWindow::paintLoop));
    timer->start(std::chrono::milliseconds(TIMEOUT_MS));
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
    // Set libalpm callbacks
    alpm_option_set_logcb(m_handle, cb_log, NULL);
    alpm_option_set_dlcb(m_handle, cb_download, NULL);
    alpm_option_set_progresscb(m_handle, cb_progress, NULL);
    alpm_option_set_eventcb(m_handle, cb_event, NULL);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

    QStringList column_names;
    column_names << "Choose"
                 << "PkgName"
                 << "Version"
                 << "Category";
    m_ui->treeKernels->setHeaderLabels(column_names);
    m_ui->treeKernels->hideColumn(TreeCol::Displayed);  // Displayed status true/false
    m_ui->treeKernels->hideColumn(TreeCol::Immutable);  // Immutable status true/false
    m_ui->treeKernels->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_ui->treeKernels->setContextMenuPolicy(Qt::CustomContextMenu);

    m_ui->treeKernels->blockSignals(true);

    // TODO(vnepogodin): parallelize it
    auto a2 = std::async(std::launch::deferred, [&] {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto& kernel : m_kernels) {
            auto widget_item = new QTreeWidgetItem(m_ui->treeKernels);
            widget_item->setCheckState(TreeCol::Check, Qt::Unchecked);
            widget_item->setText(TreeCol::PkgName, kernel.get_raw());
            widget_item->setText(TreeCol::Version, kernel.version().c_str());
            widget_item->setText(TreeCol::Category, kernel.category().data());
            widget_item->setText(TreeCol::Displayed, QStringLiteral("true"));
            if (kernel.is_installed()) {
                widget_item->setText(TreeCol::Immutable, QStringLiteral("true"));
                widget_item->setCheckState(TreeCol::Check, Qt::Checked);
            }
        }
    });

    // Connect buttons signal
    connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(on_execute()));

    // Connect worker thread signals
    connect(m_worker_th, SIGNAL(finished()), m_worker, SLOT(deleteLater()));
    connect(m_worker_th, SIGNAL(started()), m_worker, SLOT(doHeavyCaclulations()), Qt::QueuedConnection);

    // check/uncheck tree items space-bar press or double-click
    auto* shortcutToggle = new QShortcut(Qt::Key_Space, this);
    connect(shortcutToggle, &QShortcut::activated, this, &MainWindow::checkUncheckItem);

    // Connect tree widget
    connect(m_ui->treeKernels, &QTreeWidget::itemChanged, this, &MainWindow::item_changed);
    connect(m_ui->treeKernels, &QTreeWidget::itemDoubleClicked, [&](QTreeWidgetItem* item) { m_ui->treeKernels->setCurrentItem(item); });
    connect(m_ui->treeKernels, &QTreeWidget::itemDoubleClicked, this, &MainWindow::checkUncheckItem);

    // Wait for async function to finish
    a2.wait();
    m_ui->treeKernels->blockSignals(false);
}

MainWindow::~MainWindow() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (m_worker_th != nullptr) {
        m_worker_th->exit();
    }

    // Release libalpm handle
    alpm_release(m_handle);
}

void MainWindow::checkUncheckItem() noexcept {
    if (auto t_widget = qobject_cast<QTreeWidget*>(focusWidget())) {
        if (t_widget->currentItem() == nullptr || t_widget->currentItem()->childCount() > 0)
            return;
        const int col  = static_cast<int>(TreeCol::Check);
        auto new_state = (t_widget->currentItem()->checkState(col)) ? Qt::Unchecked : Qt::Checked;
        t_widget->currentItem()->setCheckState(col, new_state);
    }
}

// When selecting on item in the list
void MainWindow::item_changed(QTreeWidgetItem* item, int) noexcept {
    if (item->checkState(TreeCol::Check) == Qt::Checked)
        m_ui->treeKernels->setCurrentItem(item);
    buildChangeList(item);
}

// Build the change_list when selecting on item in the tree
void MainWindow::buildChangeList(QTreeWidgetItem* item) noexcept {
    auto item_text = item->text(TreeCol::PkgName);
    auto immutable = item->text(TreeCol::Immutable);
    if (immutable == "true" && item->checkState(0) == Qt::Unchecked) {
        m_ui->ok->setEnabled(true);
        m_change_list.append(item_text);
        return;
    }

    if (item->checkState(0) == Qt::Checked) {
        m_ui->ok->setEnabled(true);
        m_change_list.append(item_text);
    } else {
        m_change_list.removeOne(item_text);
    }

    if (m_change_list.isEmpty()) {
        m_ui->ok->setEnabled(false);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Exit worker thread
    m_running        = false;
    m_thread_running = false;

    // Execute parent function
    QWidget::closeEvent(event);
}

#ifndef PKG_DUMMY_IMPL
void MainWindow::paintLoop() noexcept {
    m_ui->progressBar->setValue(m_last_percent);
    m_ui->progress_status->setText(m_last_text);
}
#endif

void MainWindow::on_cancel() noexcept {
    close();
}

void Work::doHeavyCaclulations() {
    m_func();
}

void MainWindow::on_execute() noexcept {
    if (m_running)
        return;
    m_running = true;
    m_worker_th->start();
}
