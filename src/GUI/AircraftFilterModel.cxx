// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftFilterModel.hxx"

#include <QAbstractProxyModel>
#include <QDebug>
#include <QSettings>

#include "AircraftItemModel.hxx"
#include "SettingsWrapper.hxx"

AircraftFilterModel::AircraftFilterModel(QObject* pr, QAbstractItemModel* source)
    : QSortFilterProxyModel(pr)
{
    m_ratings = {4, 4, 4, 4};
    setSourceModel(source);

    updateSorting();

    connect(this, &QAbstractItemModel::rowsInserted, this, &AircraftFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AircraftFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AircraftFilterModel::countChanged);
}

AircraftItemModel* AircraftFilterModel::resolveItemModel() const
{
    auto* itemModel = qobject_cast<AircraftItemModel*>(sourceModel());
    if (!itemModel) {
        // Source is a proxy (e.g. AircraftSearchModel); look one level deeper.
        auto* proxySource = qobject_cast<QAbstractProxyModel*>(sourceModel());
        Q_ASSERT(proxySource);
        itemModel = qobject_cast<AircraftItemModel*>(proxySource->sourceModel());
    }
    Q_ASSERT(itemModel);
    return itemModel;
}

void AircraftFilterModel::setRatings(QList<int> ratings)
{
    if (ratings == m_ratings)
        return;
    m_ratings = ratings;
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
}

int AircraftFilterModel::indexForURI(QUrl uri) const
{
    auto* itemModel = qobject_cast<AircraftItemModel*>(sourceModel());
    QModelIndex sourceIdx;

    if (itemModel) {
        // Direct chain: AircraftItemModel → AircraftFilterModel
        sourceIdx = mapFromSource(itemModel->indexOfAircraftURI(uri));
    } else {
        // Indirect chain through a proxy (e.g. AircraftSearchModel)
        auto* proxySource = qobject_cast<QAbstractProxyModel*>(sourceModel());
        Q_ASSERT(proxySource);
        itemModel = qobject_cast<AircraftItemModel*>(proxySource->sourceModel());
        Q_ASSERT(itemModel);
        auto proxyIdx = proxySource->mapFromSource(itemModel->indexOfAircraftURI(uri));
        sourceIdx = mapFromSource(proxyIdx);
    }

    if (!sourceIdx.isValid()) {
        return -1;
    }

    return sourceIdx.row();
}

void AircraftFilterModel::selectVariantForAircraftURI(QUrl uri)
{
    resolveItemModel()->selectVariantForAircraftURI(uri);
}

void AircraftFilterModel::setRatingFilterEnabled(bool e)
{
    if (e == m_ratingsFilter) {
        return;
    }

    m_ratingsFilter = e;
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

void AircraftFilterModel::setFilteringEnabled(bool e)
{
    if (e == m_filteringEnabled) {
        return;
    }

    m_filteringEnabled = e;
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

void AircraftFilterModel::setCompatibilityFilterEnabled(bool e)
{
    if (e == m_compatibilityFilter) {
        return;
    }

    m_compatibilityFilter = e;
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

QString AircraftFilterModel::summaryText() const
{
    const int unfilteredCount = sourceModel()->rowCount();
    if (m_filteringEnabled && (unfilteredCount != rowCount())) {
        return tr("(%1 of %2 aircraft)").arg(rowCount()).arg(unfilteredCount);
    }

    return tr("(%1 aircraft)").arg(unfilteredCount);
}

int AircraftFilterModel::count() const
{
    return rowCount();
}

bool AircraftFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    // compatibility filter is independent of the main filteringEnabled flag
    if (m_compatibilityFilter) {
        QVariant v = index.data(AircraftCompatibleRole);
        if (!v.toBool()) {
            return false;
        }
    }

    if (m_filteringEnabled && m_ratingsFilter) {
        for (int i = 0; i < m_ratings.size(); ++i) {
            if (m_ratings.at(i) > index.data(AircraftRatingRole + i).toInt()) {
                return false;
            }
        }
    }

    return true;
}

void AircraftFilterModel::loadCompatibilityAndRatingsSettings()
{
    auto settings = flightgear::getQSettings();
    m_compatibilityFilter = settings.value("enable-compatibility-filter", true).toBool();
    m_ratingsFilter = settings.value("enable-ratings-filter", true).toBool();
    QVariantList vRatings = settings.value("ratings-filter").toList();
    if (vRatings.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            m_ratings[i] = vRatings.at(i).toInt();
        }
    }
    m_filteringEnabled = settings.value("enable-aircraft-filter", true).toBool();

    invalidate();
}

void AircraftFilterModel::saveCompatibilityAndRatingsSettings()
{
    auto settings = flightgear::getQSettings();
    settings.setValue("enable-compatibility-filter", m_compatibilityFilter);
    settings.setValue("enable-ratings-filter", m_ratingsFilter);
    QVariantList vRatings;
    for (int i = 0; i < 4; ++i) {
        vRatings.append(m_ratings.at(i));
    }
    settings.setValue("ratings-filter", vRatings);
    qInfo() << "Did save ratings";
}

int AircraftFilterModel::filteredOutCount() const
{
    return sourceModel()->rowCount() - rowCount();
}

AircraftFilterModel::Sorting AircraftFilterModel::sorting() const
{
    return m_sorting;
}

void AircraftFilterModel::setSorting(Sorting s)
{
    if (s == m_sorting) {
        return;
    }

    m_sorting = s;
    updateSorting();
    emit sortingChanged();
}

void AircraftFilterModel::updateSorting()
{
    switch (m_sorting) {
    case Sorting::SortByName:
        setSortRole(AircraftVariantDescriptionRole);
        setSortCaseSensitivity(Qt::CaseInsensitive);
        break;
    case Sorting::SortByRating:
        setSortRole(AircraftRatingRole);
        setSortCaseSensitivity(Qt::CaseSensitive); // doesn't matter, it's an int
        break;
    }

    setDynamicSortFilter(true);
    sort(0);
}

bool AircraftFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString variantLeft = left.data(AircraftVariantDescriptionRole).toString();
    const QString variantRight = right.data(AircraftVariantDescriptionRole).toString();

    // Sort by variant description; when equal, break ties by URI for stable ordering.
    const int c = QString::compare(variantLeft, variantRight, Qt::CaseInsensitive);
    if (c == 0) {
        const QString uriLeft = left.data(AircraftURIRole).toString();
        const QString uriRight = right.data(AircraftURIRole).toString();
        return QString::localeAwareCompare(uriLeft, uriRight) < 0;
    } else {
        return c < 0;
    }
}
