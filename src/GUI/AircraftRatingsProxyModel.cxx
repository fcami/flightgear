// SPDX-FileCopyrightText: 2026 james Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftRatingsProxyModel.hxx"

#include "AircraftItemModel.hxx"

AircraftRatingsProxyModel::AircraftRatingsProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

QVariant AircraftRatingsProxyModel::data(const QModelIndex& index, int role) const
{
    if (role == AircraftCombinedRatingRole) {
        QModelIndex srcIndex = mapToSource(index);
        double sum = 0;
        for (int i = 0; i < 4; ++i) {
            QVariant v = sourceModel()->data(srcIndex, AircraftRatingRole + i);
            sum += v.toDouble();
        }
        return sum / 4.0;
    }

    return QSortFilterProxyModel::data(index, role);
}

QHash<int, QByteArray> AircraftRatingsProxyModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles[AircraftCombinedRatingRole] = "combinedRating";
    return roles;
}
