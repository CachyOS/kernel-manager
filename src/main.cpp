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

#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

#include <QApplication>
#include <QSharedMemory>

template <typename T = std::int32_t,
    typename         = std::enable_if_t<std::numeric_limits<T>::is_integer>>
constexpr std::string_view from_int(const T& num) {
    std::array<char, 10> str;
    const auto [ptr, ec] = std::to_chars(str.data(), str.data() + str.size(), num);
    std::string_view result{str.data(), ptr};
    return result;
}

constexpr std::string_view identify_code(int sig) {
    switch (sig) {
    case SIGBUS:
        return "SIGBUS";
    case SIGABRT:
        return "SIGABRT";
    case SIGSEGV:
        return "SIGSEGV";
    }

    return from_int(sig);
}

/* Obtain a backtrace and print it to stderr. */
[[noreturn]] void print_trace(int sig) {
    void* array[50];
    const int size = backtrace(array, 50);
    fprintf(stderr, "Catched by %s code.\n", identify_code(sig).data());
    fprintf(stderr, "Obtained %d stack frames.\n", size);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

bool IsInstanceAlreadyRunning(QSharedMemory& memoryLock) {
    if (!memoryLock.create(1)) {
        memoryLock.attach();
        memoryLock.detach();

        if (!memoryLock.create(1)) {
            return true;
        }
    }

    return false;
}

auto main(int argc, char** argv) -> std::int32_t {
    QSharedMemory sharedMemoryLock("CachyOS-KM-lock");
    if (IsInstanceAlreadyRunning(sharedMemoryLock)) {
        return -1;
    }

    // Set application info
    QCoreApplication::setOrganizationName("CachyOS");
    QCoreApplication::setApplicationName("CachyOS-KM");

    // Set application attributes
    QApplication app(argc, argv);

    signal(SIGSEGV, print_trace);
    signal(SIGABRT, print_trace);
    signal(SIGBUS, print_trace);

    MainWindow w;
    w.show();
    return app.exec();
}
