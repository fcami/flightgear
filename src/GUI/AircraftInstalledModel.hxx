// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include <QUrl>

class AircraftItemModel;

/**
 * Trivial proxy that shows only aircraft that are installed locally.
 */
class AircraftInstalledModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AircraftInstalledModel(QObject* pr, QAbstractItemModel* source);

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_INVOKABLE int indexForURI(QUrl uri) const;
    Q_INVOKABLE void selectVariantForAircraftURI(QUrl uri);

    int count() const;

signals:
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};
