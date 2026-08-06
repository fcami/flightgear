// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <QAbstractListModel>

/**
 * @brief Enumerates the aircraft catalog tags known to the launcher.
 *
 * The set of valid tags, their grouping and their translated display names
 * come from FGData's Translations/default/tags.xml (itself generated from
 * fgmeta-python's catalogTags.py). This model parses that file once, at
 * construction, to discover which tags exist and how they are grouped; the
 * actual (localized) display name and description text are then fetched on
 * demand via FGLocale, so language switches are picked up automatically.
 */
class AircraftTagsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        TagRole = Qt::UserRole + 1, ///< raw tag id, e.g. "de-havilland"
        NameRole,                   ///< translated display name
        DescriptionRole,            ///< translated description (may be empty)
        GroupRole                   ///< group id, e.g. "manufacturer"
    };
    Q_ENUM(Roles)

    explicit AircraftTagsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString nameForTag(QString tag) const;
    Q_INVOKABLE QString descriptionForTag(QString tag) const;
    Q_INVOKABLE int indexOfTag(QString tag) const;
    Q_INVOKABLE bool isKnownTag(QString tag) const;

private:
    struct TagInfo {
        std::string tagId;         // e.g. "de-havilland"
        std::string group;         // e.g. "manufacturer"
        std::string translationId; // e.g. "tag-manufacturer-de-havilland"
    };

    void reload();
    const TagInfo* infoForTag(const QString& tag) const;

    std::vector<TagInfo> _tags;
};
