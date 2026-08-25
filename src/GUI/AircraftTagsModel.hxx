// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <QAbstractListModel>
#include <QColor>
#include <QSortFilterProxyModel>
#include <QStringList>

class QQmlEngine;

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
        GroupRole,                  ///< group id, e.g. "manufacturer"
        ColorRole                   ///< per-group category color (see loadStyleColors())
    };
    Q_ENUM(Roles)

    explicit AircraftTagsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QString nameForTag(QString tag) const;
    Q_INVOKABLE QString descriptionForTag(QString tag) const;
    Q_INVOKABLE QColor colorForTag(QString tag) const;
    Q_INVOKABLE int indexOfTag(QString tag) const;
    Q_INVOKABLE bool isKnownTag(QString tag) const;

    /**
     * @brief Load the per-tag-category colors from the QML Style singleton.
     *
     * Mirrors QmlColoredImageProvider::loadStyleColors(): must be called
     * once the QML engine and the Style singleton both exist (i.e. after
     * LauncherController::initQML()), reading Style.qml's
     * 'tagCategoryColors' array.
     */
    void loadStyleColors(QQmlEngine* engine, int styleTypeId);

private:
    struct TagInfo {
        std::string tagId;         // e.g. "de-havilland"
        std::string group;         // e.g. "manufacturer"
        std::string translationId; // e.g. "tag-manufacturer-de-havilland"
    };

    void reload();
    const TagInfo* infoForTag(const QString& tag) const;
    QColor colorForGroup(const std::string& group) const;

    std::vector<TagInfo> _tags;
    std::vector<QColor> _categoryColors;
};

/**
 * @brief Proxy-filter model for narrowing down AircraftTagsModel from QML.
 *
 * Used by the tag-picker UI (TagFilterPanel.qml) so the free-text/excluded-
 * tag filtering it needs (previously done row-by-row in QML JS via a
 * delegate 'visible' binding) is instead done in C++ via the usual
 * QSortFilterProxyModel::filterAcceptsRow() override.
 */
class AircraftTagsFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList excludedTags READ excludedTags WRITE setExcludedTags NOTIFY excludedTagsChanged)
public:
    explicit AircraftTagsFilterModel(QObject* parent = nullptr);

    QString filterText() const { return _filterText; }
    void setFilterText(QString filterText);

    QStringList excludedTags() const { return _excludedTags; }
    void setExcludedTags(QStringList excludedTags);

    /**
     * @brief The raw tag id of the sole matching row, or an empty string if
     * zero or more than one row currently passes the filter.
     */
    Q_INVOKABLE QString singleMatchTag() const;

signals:
    void filterTextChanged();
    void excludedTagsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString _filterText;
    QStringList _excludedTags;
};
