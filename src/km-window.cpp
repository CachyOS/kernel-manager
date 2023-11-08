// Copyright (C) 2022-2023 Vladislav Nepogodin
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
#include <QFutureWatcher>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QtConcurrent/QtConcurrent>

namespace {
bool install_packages(alpm_handle_t* handle, const std::span<Kernel>& kernels, const std::span<std::string>& selected_list) {
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if ((kernel != kernels.end()) && (!kernel->is_installed() || kernel->is_update_available())) {
            if (!kernel->install()) {
                fmt::print(stderr, "failed to add package to be installed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }
    return true;
}

bool remove_packages(alpm_handle_t* handle, const std::span<Kernel>& kernels, const std::span<std::string>& selected_list) {
    for (const auto& selected : selected_list) {
        const auto& kernel = ranges::find_if(kernels, [selected](auto&& el) { return el.get_raw() == selected; });
        if ((kernel != kernels.end()) && (kernel->is_installed())) {
            if (!kernel->remove()) {
                fmt::print(stderr, "failed to add package to be removed ({})\n", alpm_strerror(alpm_errno(handle)));
            }
        }
    }

    return true;
}

bool is_kernels_change_state(alpm_handle_t* handle, std::span<std::string_view> kernel_install_list, std::span<std::string_view> kernel_removal_list) {
    if (handle == nullptr) {
        return false;
    }
    auto* local_db = alpm_get_localdb(handle);

    for (auto&& kernel_install : kernel_install_list) {
        auto* pkg = alpm_db_get_pkg(local_db, kernel_install.data());
        if (pkg != nullptr) {
            return true;
        }
    }
    for (auto&& kernel_removal : kernel_removal_list) {
        auto* pkg = alpm_db_get_pkg(local_db, kernel_removal.data());
        if (pkg == nullptr) {
            return true;
        }
    }
    return false;
}

void init_kernels_tree_widget(QTreeWidget* tree_kernels, std::span<Kernel> kernels) noexcept {
    for (auto& kernel : kernels) {
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
}
}  // namespace

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent) {
    m_ui->setupUi(this);

    setAttribute(Qt::WA_NativeWindow);
    setWindowFlags(Qt::Window);  // for the close, min and max buttons

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

                install_packages(m_handle, m_kernels, change_list);
                remove_packages(m_handle, m_kernels, change_list);
                Kernel::commit_transaction();

                // check if we need to re-init kernels
                // [1.1]
                auto& kernel_install_list = Kernel::get_install_list();
                auto& kernel_removal_list = Kernel::get_removal_list();

                // NOTE: we don't want to override handle, because we would need to invalidate kernels then.
                auto* temp_handle = utils::parse_alpm("/", "/var/lib/pacman/", &m_err);
                if (temp_handle == nullptr) {
                    QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Failed to initialize alpm handle (%1)").arg(alpm_strerror(m_err)));
                }

                // [1.2]
                // iterate over install and removal lists and check if any of the packages
                // in the lists were either installed or removed
                const bool is_kernel_status_changed = is_kernels_change_state(temp_handle, std::span{kernel_install_list}, std::span{kernel_removal_list});

                // [1.3]
                // if kernel status has changed, then re-init alpm handler,
                // fetch kernels and repopulate tree widget again
                if (is_kernel_status_changed) {
                    if (m_handle != nullptr && utils::release_alpm(m_handle, &m_err) != 0) {
                        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Failed to release alpm handle (%1)").arg(alpm_strerror(m_err)));
                    }

                    m_handle = temp_handle;
                    m_kernels.clear();
                    m_kernels = Kernel::get_kernels(m_handle);

                    // schedule init_kernels to be executed in the main thread
                    QMetaObject::invokeMethod(this, "init_kernels", Qt::QueuedConnection);
                }

                // clear install and removal lists
                kernel_install_list.clear();
                kernel_removal_list.clear();

                m_running.store(false, std::memory_order_relaxed);
                m_ui->ok->setEnabled(!is_kernel_status_changed);
            }
        }
    });

    m_worker->moveToThread(m_worker_th);
    // name to appear in ps, task manager, etc.
    m_worker_th->setObjectName("WorkerThread");

    m_ui->ok->setEnabled(false);

    // Setup progress dialog
    set_progress_dialog();

    // Setup configure window
    connect(&m_future_watcher, &QFutureWatcher<void>::finished, this, [&]() {
        m_conf_progress_dialog->hide();
        if (m_future_watcher.future().isCanceled()) {
            return;
        }
        if (m_future_watcher.future().isFinished()) {
            m_conf_window->show();
            return;
        }
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("Failed to clone repository!\nPlease check your internet connection and try again"));
    });
    connect(m_conf_progress_dialog, &QProgressDialog::canceled, this, [&]() {
        fmt::print("the operation was canceled!\n");
        // that doesn't really stop execution, it just hides the progress dialog
        m_future_watcher.cancel();
    });

    // Setup tree widget
    auto* tree_kernels = m_ui->treeKernels;
    tree_kernels->hideColumn(TreeCol::Displayed);  // Displayed status true/false
    tree_kernels->hideColumn(TreeCol::Immutable);  // Immutable status true/false
    tree_kernels->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Set context menu policy
    tree_kernels->setContextMenuPolicy(Qt::CustomContextMenu);

    tree_kernels->blockSignals(true);

    // TODO(vnepogodin): parallelize it
    auto a2 = std::async(std::launch::deferred, [&] {
        const std::lock_guard<std::mutex> guard(m_mutex);
        init_kernels_tree_widget(tree_kernels, std::span{m_kernels});
    });

    if (m_kernels.empty()) {
        QMessageBox::critical(this, "CachyOS Kernel Manager", tr("No kernels found!\nPlease run `pacman -Sy` to update DB!\nThis is needed for the app to work properly"));
    }

    // Connect buttons signal
    connect(m_ui->cancel, SIGNAL(clicked()), this, SLOT(on_cancel()));
    connect(m_ui->ok, SIGNAL(clicked()), this, SLOT(on_execute()));
    connect(m_ui->configure, SIGNAL(clicked()), this, SLOT(on_configure()));

    // Connect worker thread signals
    connect(m_worker_th, SIGNAL(finished()), m_worker, SLOT(deleteLater()));
    connect(m_worker_th, SIGNAL(started()), m_worker, SLOT(doHeavyCalculations()), Qt::QueuedConnection);

    // check/uncheck tree items space-bar press or double-click
    auto* shortcutToggle = new QShortcut(Qt::Key_Space, this);
    connect(shortcutToggle, &QShortcut::activated, this, &MainWindow::check_uncheck_item);

    // Connect tree widget
    connect(tree_kernels, &QTreeWidget::itemChanged, this, &MainWindow::item_changed);
    connect(tree_kernels, &QTreeWidget::itemDoubleClicked, [&](QTreeWidgetItem* item) { m_ui->treeKernels->setCurrentItem(item); });
    connect(tree_kernels, &QTreeWidget::itemDoubleClicked, this, &MainWindow::check_uncheck_item);

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

// Setup progress dialog
void MainWindow::set_progress_dialog() noexcept {
    m_conf_progress_dialog = new QProgressDialog(this);
    m_conf_progress_bar    = new QProgressBar(m_conf_progress_dialog);

    // Set progress dialog
    m_conf_progress_bar->setMinimum(0);
    m_conf_progress_bar->setMaximum(0);
    m_conf_progress_dialog->setMinimum(0);
    m_conf_progress_dialog->setMaximum(0);

    // Set progress dialog properties
    m_conf_progress_dialog->setWindowModality(Qt::WindowModal);
    m_conf_progress_dialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint | Qt::WindowStaysOnTopHint);
    m_conf_progress_dialog->setLabelText(tr("Please wait...\nWe are preparing configuration window for you\ncloning PKGBUILDs.."));
    m_conf_progress_dialog->setAutoClose(false);
    m_conf_progress_dialog->setBar(m_conf_progress_bar);
    m_conf_progress_bar->setTextVisible(false);
    m_conf_progress_dialog->reset();
}

void MainWindow::check_uncheck_item() noexcept {
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
    build_change_list(item);
}

// Build the change_list when selecting on item in the tree
void MainWindow::build_change_list(QTreeWidgetItem* item) noexcept {
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

void MainWindow::on_configure() noexcept {
    // show progress dialog to indicate user something is happening
    m_conf_progress_dialog->setLabelText(tr("Please wait...\nWe are preparing configuration window for you\ncloning PKGBUILDs.."));
    m_conf_progress_dialog->show();

    // NOTE: the future created by QtConcurrent::run is not cancelable.
    // prepare in the background, without blocking the UI
    m_future_watcher.setFuture(QtConcurrent::run([this] {
        utils::prepare_build_environment();
        m_conf_window->reset_patches_data_tab();
    }));
}

void MainWindow::on_cancel() noexcept {
    close();
}

void Work::doHeavyCalculations() {
    m_func();
}

void MainWindow::init_kernels() noexcept {
    // show progress dialog to indicate user something is happening
    m_conf_progress_dialog->setLabelText(tr("Please wait...\nInitializing kernels.."));
    m_conf_progress_dialog->show();

    auto* tree_kernels = m_ui->treeKernels;
    tree_kernels->blockSignals(true);
    tree_kernels->clear();

    // NOTE: I don't think this should be parallelized, because it's already not running on the main thread
    init_kernels_tree_widget(tree_kernels, std::span{m_kernels});

    tree_kernels->blockSignals(false);
    m_conf_progress_dialog->hide();
}

void MainWindow::on_execute() noexcept {
    if (m_running.load(std::memory_order_consume))
        return;
    m_running.store(true, std::memory_order_relaxed);
    m_thread_running.store(true, std::memory_order_relaxed);
    m_cv.notify_all();
    m_worker_th->start();
}
