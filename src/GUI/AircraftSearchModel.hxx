// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include <QUrl>

#include <simgear/props/props.hxx>

/**
 * Proxy model that filters aircraft based on a text search string.
 *
 * This model is typically the innermost proxy in the browse chain, sitting
 * between the raw AircraftItemModel and an AircraftFilterModel:
 *
 *   AircraftItemModel → AircraftSearchModel → AircraftFilterModel
 *
 * Use setSearchString() to update the active search term. An empty string
 * disables filtering and all aircraft pass through.
 */
class AircraftSearchModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AircraftSearchModel(QObject* pr, QAbstractItemModel* source);

    Q_PROPERTY(QString searchString READ searchString NOTIFY searchStringChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_INVOKABLE void setSearchString(QString s);

    QString searchString() const
    {
        return m_searchString;
    }

    int count() const;

signals:
    void searchStringChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_searchString;
    SGPropertyNode_ptr m_searchProps;
};
