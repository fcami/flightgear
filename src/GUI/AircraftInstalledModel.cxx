// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftInstalledModel.hxx"

#include "AircraftItemModel.hxx"

AircraftInstalledModel::AircraftInstalledModel(QObject* pr, QAbstractItemModel* source)
    : QSortFilterProxyModel(pr)
{
    setSourceModel(source);
    connect(this, &QAbstractItemModel::rowsInserted, this, &AircraftInstalledModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AircraftInstalledModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AircraftInstalledModel::countChanged);
}

int AircraftInstalledModel::count() const
{
    return rowCount();
}

bool AircraftInstalledModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto status = static_cast<LocalAircraftCache::PackageStatus>(
        index.data(AircraftPackageStatusRole).toInt());
    return status != LocalAircraftCache::PackageNotInstalled;
}
