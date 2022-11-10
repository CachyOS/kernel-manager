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
#include "conf-window.hpp"
#include "kernel.hpp"
#include "utils.hpp"

#include <future>
#include <span>
#include <thread>

#include <fmt/core.h>

#include <QCoreApplication>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QTreeWidgetItem>

#ifndef PKG_DUMMY_IMPL
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
    case ALPM_EVENT_PKG_RETRIEVE_FAILED:
        opr             = "Failed to retrieve package.";
        *g_last_percent = 0;
        break;
    // all the simple done events, with fallthrough for each
    case ALPM_EVENT_OPTDEP_REMOVAL:
    case ALPM_EVENT_DB_RETRIEVE_START:
    case ALPM_EVENT_PACNEW_CREATED:
    case ALPM_EVENT_PACSAVE_CREATED:
    case ALPM_EVENT_DB_RETRIEVE_DONE:
    case ALPM_EVENT_DB_RETRIEVE_FAILED:
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
    using namespace std::literals::string_view_literals;

    (void)ctx;
    std::string_view opr{"processing"};
    // set text of message to display
    switch (event) {
    case ALPM_PROGRESS_ADD_START:
        opr = "installing"sv;
        break;
    case ALPM_PROGRESS_UPGRADE_START:
        opr = "upgrading"sv;
        break;
    case ALPM_PROGRESS_DOWNGRADE_START:
        opr = "downgrading"sv;
        break;
    case ALPM_PROGRESS_REINSTALL_START:
        opr = "reinstalling"sv;
        break;
    case ALPM_PROGRESS_REMOVE_START:
        opr = "removing"sv;
        break;
    case ALPM_PROGRESS_CONFLICTS_START:
        opr = "checking for file conflicts"sv;
        break;
    case ALPM_PROGRESS_DISKSPACE_START:
        opr = "checking available disk space"sv;
        break;
    case ALPM_PROGRESS_INTEGRITY_START:
        opr = "checking package integrity"sv;
        break;
    case ALPM_PROGRESS_KEYRING_START:
        opr = "checking keys in keyring"sv;
        break;
    case ALPM_PROGRESS_LOAD_START:
        opr = "loading package files"sv;
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

static std::string clean_filename(const std::string_view& filename) {
    using namespace std::literals::string_view_literals;

    std::string_view fname = filename;
    // strip package or DB extension for cleaner look
    static constexpr std::array strip_extensions{".pkg"sv, ".db"sv, ".files"sv};
    for (auto extension : strip_extensions) {
        auto trim_pos = fname.find(extension);
        if (trim_pos != std::string_view::npos) {
            fname.remove_suffix(fname.size() - trim_pos);
            return std::string(fname.begin(), fname.end());
        }
    }

    return std::string(fname.begin(), fname.end());
}

/* Handles download progress event */
static void dload_progress_event(const char* filename, alpm_download_event_progress_t* data) {
    (void)filename;

    const auto& percent = (data->downloaded * 100) / data->total;
    *g_last_percent     = static_cast<int32_t>(percent);
}

/* download completed */
static void dload_complete_event(const char* filename, alpm_download_event_completed_t* data) {
    auto cleaned_filename = clean_filename(filename);

    if (data->result == 1) {
        // The line contains text from previous status. Erase these leftovers.
        *g_last_text    = fmt::format("{} is up to date", cleaned_filename).c_str();
        *g_last_percent = 0;
    } else if (data->result == 0) {
        *g_last_text = fmt::format("{} download completed successfully", cleaned_filename).c_str();
    } else {
        *g_last_text = fmt::format("{} failed to download", cleaned_filename).c_str();
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
        auto cleaned_filename = clean_filename(filename);
        *g_last_percent       = 0;
        *g_last_text          = fmt::format("{} downloading...", cleaned_filename).c_str();
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
    if (!fmt || fmt[0] == '\0') {
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
#endif

bool install_packages(alpm_handle_t* handle, const std::span<Kernel>& kernels, const std::span<std::string>& selected_list) {
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
        return false;
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
        return false;
    }

    /* Step 3: actually perform the installation */
    const auto* inst_packages = alpm_trans_get_add(handle);
    if (inst_packages == nullptr) {
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return false;
    }

    if (alpm_trans_commit(handle, &trans_data) != 0) {
        *g_last_text    = fmt::format("failed to commit transaction ({})", alpm_strerror(alpm_errno(handle))).c_str();
        *g_last_percent = 0;
        fmt::print(stderr, "{}\n", g_last_text->toStdString());
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return false;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
#endif
    return true;
}

bool remove_packages(alpm_handle_t* handle, const std::span<Kernel>& kernels, const std::span<std::string>& selected_list) {
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
        return false;
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
        return false;
    }

    /* Step 3: actually perform the removal */
    const auto* remove_packages = alpm_trans_get_remove(handle);
    if (remove_packages == nullptr) {
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return false;
    }

    if (alpm_trans_commit(handle, &trans_data) != 0) {
        *g_last_text    = fmt::format("failed to commit transaction ({})", alpm_strerror(alpm_errno(handle))).c_str();
        *g_last_percent = 0;
        fmt::print(stderr, "{}\n", g_last_text->toStdString());
        alpm_list_free(trans_data);
        alpm_trans_release(handle);
        return false;
    }

    /* Step 4: release transaction resources */
    FREELIST(trans_data);
    alpm_trans_release(handle);
#endif
    return true;
}

#ifndef PKG_DUMMY_IMPL
void setup_handlers(alpm_handle_t* handle) noexcept {

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
    // Set libalpm callbacks
    alpm_option_set_logcb(handle, cb_log, nullptr);
    alpm_option_set_dlcb(handle, cb_download, nullptr);
    alpm_option_set_progresscb(handle, cb_progress, nullptr);
    alpm_option_set_eventcb(handle, cb_event, nullptr);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
}
#endif

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

    // Setup global values
#ifdef PKG_DUMMY_IMPL
    m_ui->progress_status->hide();
    m_ui->progressBar->hide();
#else
    g_last_text    = &m_last_text;
    g_last_percent = &m_last_percent;
#endif

    // Create worker thread
    m_worker = new Work([&]() {
        while (m_thread_running.load(std::memory_order_consume)) {
            std::unique_lock<std::mutex> lock(m_mutex);
            fmt::print(stderr, "Waiting... \n");

            m_cv.wait(lock, [&] { return m_running.load(std::memory_order_consume); });

            if (m_running.load(std::memory_order_consume) && m_thread_running.load(std::memory_order_consume)) {
                m_ui->ok->setEnabled(false);

                std::vector<std::string> change_list(static_cast<std::size_t>(m_change_list.size()));
                for (int i = 0; i < m_change_list.size(); ++i) {
                    change_list[static_cast<std::size_t>(i)] = m_change_list[i].toStdString();
                }

#ifdef PKG_DUMMY_IMPL
                install_packages(m_handle, m_kernels, change_list);
                remove_packages(m_handle, m_kernels, change_list);
                Kernel::commit_transaction();
#else
                if (install_packages(m_handle, m_kernels, change_list)) {
                    m_last_percent = 100;
                    m_last_text    = "Done";

                    utils::release_alpm(m_handle, &m_err);
                    m_handle = utils::parse_alpm("/", "/var/lib/pacman/", &m_err);
                    setup_handlers(m_handle);

                    fmt::print(stderr, "the install has been performed!\n");
                }
                if (remove_packages(m_handle, m_kernels, change_list)) {
                    m_last_percent = 100;
                    m_last_text    = "Done";

                    utils::release_alpm(m_handle, &m_err);
                    m_handle = utils::parse_alpm("/", "/var/lib/pacman/", &m_err);
                    setup_handlers(m_handle);

                    fmt::print(stderr, "the removal has been performed!\n");
                }

                // TODO(vnepogodin): if our actual data has changed,
                // then update UI tree. Update checkboxes state, etc.
#endif

                m_running.store(false, std::memory_order_relaxed);
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

    setup_handlers(m_handle);
#endif

    auto* tree_kernels = m_ui->treeKernels;
    QStringList column_names;
    column_names << "Choose"
                 << "PkgName"
                 << "Version"
                 << "Category";
    tree_kernels->setHeaderLabels(column_names);
    tree_kernels->hideColumn(TreeCol::Displayed);  // Displayed status true/false
    tree_kernels->hideColumn(TreeCol::Immutable);  // Immutable status true/false
    tree_kernels->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    tree_kernels->setContextMenuPolicy(Qt::CustomContextMenu);

    tree_kernels->blockSignals(true);

    // TODO(vnepogodin): parallelize it
    auto a2 = std::async(std::launch::deferred, [&] {
        std::lock_guard<std::mutex> guard(m_mutex);
        for (auto& kernel : m_kernels) {
            auto widget_item = new QTreeWidgetItem(tree_kernels);
            widget_item->setCheckState(TreeCol::Check, Qt::Unchecked);
            widget_item->setText(TreeCol::PkgName, kernel.get_raw());
            widget_item->setText(TreeCol::Version, kernel.version().c_str());
            widget_item->setText(TreeCol::Category, kernel.category().data());
            widget_item->setText(TreeCol::Displayed, QStringLiteral("true"));
            if (kernel.is_installed()) {
                const std::string_view kernel_installed_db = kernel.get_installed_db();
                if (!kernel_installed_db.empty() && kernel_installed_db != kernel.get_repo()) {
                    continue;
                }
                widget_item->setText(TreeCol::Immutable, QStringLiteral("true"));
                widget_item->setCheckState(TreeCol::Check, Qt::Checked);
            }
        }
    });

    // Connect buttons signal
    connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(on_execute()));
    connect(m_ui->configure, SIGNAL(clicked()), this, SLOT(on_configure()));

    // Connect worker thread signals
    connect(m_worker_th, SIGNAL(finished()), m_worker, SLOT(deleteLater()));
    connect(m_worker_th, SIGNAL(started()), m_worker, SLOT(doHeavyCalculations()), Qt::QueuedConnection);

    // check/uncheck tree items space-bar press or double-click
    auto* shortcutToggle = new QShortcut(Qt::Key_Space, this);
    connect(shortcutToggle, &QShortcut::activated, this, &MainWindow::checkUncheckItem);

    // Connect tree widget
    connect(tree_kernels, &QTreeWidget::itemChanged, this, &MainWindow::item_changed);
    connect(tree_kernels, &QTreeWidget::itemDoubleClicked, [&](QTreeWidgetItem* item) { m_ui->treeKernels->setCurrentItem(item); });
    connect(tree_kernels, &QTreeWidget::itemDoubleClicked, this, &MainWindow::checkUncheckItem);

    // Wait for async function to finish
    a2.wait();
    tree_kernels->blockSignals(false);
}

MainWindow::~MainWindow() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (m_worker_th != nullptr) {
        m_worker_th->exit();
    }
}

void MainWindow::checkUncheckItem() noexcept {
    if (auto t_widget = qobject_cast<QTreeWidget*>(focusWidget())) {
        if (t_widget->currentItem() == nullptr || t_widget->currentItem()->childCount() > 0) {
            return;
        }
        auto new_state = (t_widget->currentItem()->checkState(TreeCol::Check) == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
        t_widget->currentItem()->setCheckState(TreeCol::Check, new_state);
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
    m_running.store(true, std::memory_order_relaxed);
    m_thread_running.store(false, std::memory_order_relaxed);
    m_cv.notify_all();

    // Release libalpm handle
    alpm_release(m_handle);

    // Execute parent function
    QWidget::closeEvent(event);
}

#ifndef PKG_DUMMY_IMPL
void MainWindow::paintLoop() noexcept {
    m_ui->progressBar->setValue(m_last_percent);
    m_ui->progress_status->setText(m_last_text);
}
#endif

void MainWindow::on_configure() noexcept {
    m_confwindow->show();
}

void MainWindow::on_cancel() noexcept {
    close();
}

void Work::doHeavyCalculations() {
    m_func();
}

void MainWindow::on_execute() noexcept {
    if (m_running.load(std::memory_order_consume))
        return;
    m_running.store(true, std::memory_order_relaxed);
    m_thread_running.store(true, std::memory_order_relaxed);
    m_cv.notify_all();
    m_worker_th->start();
}
