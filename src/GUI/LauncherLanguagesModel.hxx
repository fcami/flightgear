// SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QString>

/**
 * @brief Lists the languages available for the launcher / simulator UI:
 * a first "use system language" entry (empty code, sentinel for "no
 * override"), followed by one entry per language compiled into the
 * launcher (see FG_LAUNCHER_LANGUAGES in config.h).
 *
 * Owns persistence of the current selection directly (QSettings "language"
 * key) rather than going through the usual SettingControl save/restore
 * cycle, since the value needs to be readable very early at startup,
 * before any QML exists.
 */
class LauncherLanguagesModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentLanguageCode READ currentLanguageCode NOTIFY currentIndexChanged)

public:
    explicit LauncherLanguagesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const { return m_currentIndex; }

    /**
     * @return the language code of the selected entry, or the empty string
     *         for the "use system language" entry.
     */
    QString currentLanguageCode() const;

public slots:
    void setCurrentIndex(int index);

signals:
    void currentIndexChanged(int index);

private:
    struct LanguageInfo {
        QString code; // empty for the "use system language" sentinel entry
        QString name; // display name
    };

    void restoreSelection();

    std::vector<LanguageInfo> m_languages;
    int m_currentIndex = 0;
};
