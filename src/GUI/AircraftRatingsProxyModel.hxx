// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>

/**
 * Proxy model that adds a combined/average rating to each aircraft row.
 *
 * The combined rating is computed from the four existing ratings
 * (AircraftRatingRole .. AircraftRatingRole+3) and exposed via
 * AircraftCombinedRatingRole.
 */
class AircraftRatingsProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit AircraftRatingsProxyModel(QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role) const override;

    QHash<int, QByteArray> roleNames() const override;
};
