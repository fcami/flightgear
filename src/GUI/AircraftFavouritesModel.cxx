// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftFavouritesModel.hxx"

#include "AircraftItemModel.hxx"
#include "FavouriteAircraftData.hxx"

AircraftFavouritesModel::AircraftFavouritesModel(QObject* pr, QAbstractItemModel* source)
    : QSortFilterProxyModel(pr)
{
    setSourceModel(source);
    setDynamicSortFilter(false);
    connect(this, &QAbstractItemModel::rowsInserted, this, &AircraftFavouritesModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AircraftFavouritesModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AircraftFavouritesModel::countChanged);
    connect(FavouriteAircraftData::instance(), &FavouriteAircraftData::changed,
            [this]() { this->invalidate(); });
}

int AircraftFavouritesModel::indexForURI(QUrl uri) const
{
    auto srcIdx = qobject_cast<AircraftItemModel*>(sourceModel())->indexOfAircraftURI(uri);
    auto ourIdx = mapFromSource(srcIdx);
    return ourIdx.isValid() ? ourIdx.row() : -1;
}

void AircraftFavouritesModel::selectVariantForAircraftURI(QUrl uri)
{
    qobject_cast<AircraftItemModel*>(sourceModel())->selectVariantForAircraftURI(uri);
}

int AircraftFavouritesModel::count() const
{
    return rowCount();
}

bool AircraftFavouritesModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return index.data(AircraftIsFavouriteRole).toBool();
}
