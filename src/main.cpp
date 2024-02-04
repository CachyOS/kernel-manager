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

#include "km-window.hpp"

#include <QApplication>
#include <QSharedMemory>
#include <QTranslator>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <QLibraryInfo>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

bool IsInstanceAlreadyRunning(QSharedMemory& memoryLock) noexcept {
    if (!memoryLock.create(1)) {
        memoryLock.attach();
        memoryLock.detach();

        if (!memoryLock.create(1)) {
            return true;
        }
    }

    return false;
}

/* Adopted from bitcoin-qt source code.
 * Licensed under MIT
 */
/** Set up translations */
void initTranslations(QTranslator& qtTranslatorBase, QTranslator& qtTranslator, QTranslator& translatorBase, QTranslator& translator) noexcept {
    // Remove old translators
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    const auto lang_territory = QLocale::system().name();

    // Convert to "de" only by truncating "_DE"
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    const auto translation_path{QLibraryInfo::location(QLibraryInfo::TranslationsPath)};
#else
    const auto translation_path{QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
#endif

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, translation_path)) {
        QApplication::installTranslator(&qtTranslatorBase);
    }

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, translation_path)) {
        QApplication::installTranslator(&qtTranslator);
    }

    // Load e.g. cachyos-kernel-manager_de.qm (shortcut "de" needs to be defined in bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/")) {
        QApplication::installTranslator(&translatorBase);
    }

    // Load e.g. cachyos-kernel-manager_de_DE.qm (shortcut "de_DE" needs to be defined in bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/")) {
        QApplication::installTranslator(&translator);
    }
}

}  // namespace

auto main(int argc, char** argv) -> std::int32_t {
    QSharedMemory sharedMemoryLock("CachyOS-KM-lock");
    if (IsInstanceAlreadyRunning(sharedMemoryLock)) {
        return -1;
    }

    /// 1. Basic Qt initialization (not dependent on parameters or configuration)
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    /// 2. Application identification
    QApplication::setOrganizationName("CachyOS");
    QApplication::setOrganizationDomain("cachyos.org");
    QApplication::setApplicationName("CachyOS-KM");

    // Set application attributes
    const QApplication app(argc, argv);

    /// 3. Initialization of translations
    QTranslator qtTranslatorBase;
    QTranslator qtTranslator;
    QTranslator translatorBase;
    QTranslator translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

    MainWindow w;
    w.show();
    return app.exec();  // NOLINT
}
