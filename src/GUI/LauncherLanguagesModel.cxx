// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "LauncherLanguagesModel.hxx"

#include <algorithm>

#include <QLocale>

#include "SettingsWrapper.hxx"

namespace {
const int LanguageCodeRole = Qt::UserRole + 1;
}

LauncherLanguagesModel::LauncherLanguagesModel(QObject* parent) : QAbstractListModel(parent)
{
    // row 0 is always the "use system language" sentinel entry (empty code)
    QString detectedName = QLocale::system().nativeLanguageName();
    if (detectedName.isEmpty()) {
        detectedName = QLocale::languageToString(QLocale::system().language());
    }
    m_languages.push_back({QString(), tr("Use system language (%1)").arg(detectedName)});

    std::vector<LanguageInfo> entries;
    const QStringList codes = QString::fromLatin1(FG_LAUNCHER_LANGUAGES).split(',');
    for (const QString& code : codes) {
        if (code.isEmpty())
            continue;

        QLocale locale(code);
        QString name = locale.nativeLanguageName();
        if (name.isEmpty()) {
            name = QLocale::languageToString(locale.language());
        }
        entries.push_back({code, name});
    }

    std::sort(entries.begin(), entries.end(), [](const LanguageInfo& a, const LanguageInfo& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    m_languages.insert(m_languages.end(), entries.begin(), entries.end());

    restoreSelection();
}

int LauncherLanguagesModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_languages.size());
}

QVariant LauncherLanguagesModel::data(const QModelIndex& index, int role) const
{
    const int row = index.row();
    if (!index.isValid() || (row < 0) || (static_cast<size_t>(row) >= m_languages.size()))
        return {};

    const auto& info = m_languages.at(row);
    if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
        return info.name;
    } else if (role == LanguageCodeRole) {
        return info.code;
    }

    return {};
}

QHash<int, QByteArray> LauncherLanguagesModel::roleNames() const
{
    QHash<int, QByteArray> result = QAbstractListModel::roleNames();
    result[LanguageCodeRole] = "code";
    return result;
}

QString LauncherLanguagesModel::currentLanguageCode() const
{
    if ((m_currentIndex < 0) || (static_cast<size_t>(m_currentIndex) >= m_languages.size()))
        return {};

    return m_languages.at(m_currentIndex).code;
}

void LauncherLanguagesModel::setCurrentIndex(int index)
{
    if ((index < 0) || (static_cast<size_t>(index) >= m_languages.size()) || (index == m_currentIndex))
        return;

    m_currentIndex = index;

    auto settings = flightgear::getQSettings();
    const QString code = m_languages.at(m_currentIndex).code;
    if (code.isEmpty()) {
        settings.remove("language");
    } else {
        settings.setValue("language", code);
    }

    emit currentIndexChanged(m_currentIndex);
}

void LauncherLanguagesModel::restoreSelection()
{
    const QString persisted = flightgear::getQSettings().value("language").toString();
    if (persisted.isEmpty()) {
        m_currentIndex = 0;
        return;
    }

    auto it = std::find_if(m_languages.begin(), m_languages.end(), [&persisted](const LanguageInfo& info) {
        return info.code.compare(persisted, Qt::CaseInsensitive) == 0;
    });

    m_currentIndex = (it != m_languages.end()) ? static_cast<int>(std::distance(m_languages.begin(), it)) : 0;
}
