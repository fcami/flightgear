// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include <QUrl>
#include <QVariantList>

#include <simgear/props/props.hxx>

/**
 * Proxy model that filters aircraft based on a text search string and
 * selected tags.
 *
 * This model is typically the innermost proxy in the browse chain, sitting
 * between the raw AircraftItemModel and an AircraftFilterModel:
 *
 *   AircraftItemModel → AircraftSearchModel → AircraftFilterModel
 *
 * Use setSearchString() to update the active search term. An empty string
 * disables text filtering and all aircraft pass through.
 */
class AircraftSearchModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AircraftSearchModel(QObject* pr, QAbstractItemModel* source);

    Q_PROPERTY(QString searchString READ searchString NOTIFY searchStringChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

    /**
     * List of selected tags to filter by. Each entry is a map with "tag"
     * (string) and "negated" (bool) keys: a negated entry requires the tag
     * to be *absent* rather than present. Negated entries are combined with
     * logical AND (an aircraft having any negated tag is excluded);
     * non-negated entries are combined with logical OR (an aircraft must
     * have at least one of them, if any are present).
     */
    Q_PROPERTY(QVariantList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY selectedTagsChanged)

    Q_INVOKABLE void setSearchString(QString s);

    QString searchString() const
    {
        return m_searchString;
    }

    int count() const;

    QVariantList selectedTags() const;
    void setSelectedTags(QVariantList tags);

    /// true if \p tag is currently selected (negated or not)
    Q_INVOKABLE bool hasTag(QString tag) const;

    /// add \p tag to the selection (non-negated) if not already present
    Q_INVOKABLE void addTag(QString tag);

    /// remove \p tag from the selection, if present
    Q_INVOKABLE void removeTag(QString tag);

    /// flip the negated state of \p tag, if present
    Q_INVOKABLE void toggleTagNegation(QString tag);

    Q_INVOKABLE void clearTags();

signals:
    void searchStringChanged();
    void countChanged();
    void selectedTagsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    struct SelectedTag {
        QString tag;
        bool negated = false;
    };

    QString m_searchString;
    SGPropertyNode_ptr m_searchProps;
    QList<SelectedTag> m_selectedTags;
};
