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

#ifndef CONFOPTIONSPAGE_HPP_
#define CONFOPTIONSPAGE_HPP_

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#pragma GCC diagnostic ignored "-Wsuggest-final-methods"
#endif

#include <ui_conf-options-page.h>

#include <memory>

#include <QObject>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class ConfOptionsPage final : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConfOptionsPage)
 public:
    explicit ConfOptionsPage(QWidget* parent = nullptr)
      : QWidget(parent) { m_ui->setupUi(this); }
    ~ConfOptionsPage() = default;

    Ui::ConfOptionsPage* get_ui_obj() noexcept { return m_ui.get(); }

 private:
    std::unique_ptr<Ui::ConfOptionsPage> m_ui = std::make_unique<Ui::ConfOptionsPage>();
};

#endif  // CONFOPTIONSPAGE_HPP_
