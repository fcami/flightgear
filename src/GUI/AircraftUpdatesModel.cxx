// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftUpdatesModel.hxx"

#include "AircraftItemModel.hxx"

AircraftUpdatesModel::AircraftUpdatesModel(QObject* pr, QAbstractItemModel* source)
    : QSortFilterProxyModel(pr)
{
    setSourceModel(source);
    connect(this, &QAbstractItemModel::rowsInserted, this, &AircraftUpdatesModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AircraftUpdatesModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AircraftUpdatesModel::countChanged);
}

int AircraftUpdatesModel::indexForURI(QUrl uri) const
{
    auto srcIdx = qobject_cast<AircraftItemModel*>(sourceModel())->indexOfAircraftURI(uri);
    auto ourIdx = mapFromSource(srcIdx);
    return ourIdx.isValid() ? ourIdx.row() : -1;
}

void AircraftUpdatesModel::selectVariantForAircraftURI(QUrl uri)
{
    qobject_cast<AircraftItemModel*>(sourceModel())->selectVariantForAircraftURI(uri);
}

int AircraftUpdatesModel::count() const
{
    return rowCount();
}

bool AircraftUpdatesModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto status = static_cast<LocalAircraftCache::PackageStatus>(
        index.data(AircraftPackageStatusRole).toInt());
    switch (status) {
    case LocalAircraftCache::PackageNotInstalled:
    case LocalAircraftCache::PackageInstalled:
    case LocalAircraftCache::NotPackaged:
        return false;
    default:
        return true; // update downloaded, queued, downloading, etc.
    }
}
