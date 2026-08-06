// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftFilterModel.hxx"

#include <algorithm>

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
    auto m = sourceModel();
    while (m) {
        auto* itemModel = qobject_cast<AircraftItemModel*>(m);
        if (itemModel) {
            return itemModel;
        }

        auto pm = qobject_cast<QAbstractProxyModel*>(m);
        if (!pm) {
            break;
        }

        m = pm->sourceModel();
    }

    qFatal("AircraftFilterModel::resolveItemModel() failed to find AircraftItemModel in the source model chain");
    return nullptr;
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

QVariantList AircraftFilterModel::selectedTags() const
{
    QVariantList result;
    for (const auto& t : m_selectedTags) {
        QVariantMap m;
        m["tag"] = t.tag;
        m["negated"] = t.negated;
        result.append(m);
    }
    return result;
}

void AircraftFilterModel::setSelectedTags(QVariantList tags)
{
    QList<SelectedTag> newTags;
    for (const auto& v : tags) {
        const auto m = v.toMap();
        newTags.append({m.value("tag").toString(), m.value("negated").toBool()});
    }

    m_selectedTags = newTags;
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

bool AircraftFilterModel::hasTag(QString tag) const
{
    for (const auto& t : m_selectedTags) {
        if (t.tag.compare(tag, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void AircraftFilterModel::addTag(QString tag)
{
    if (hasTag(tag)) {
        return;
    }

    m_selectedTags.append({tag, false});
    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

void AircraftFilterModel::removeTag(QString tag)
{
    const auto count = m_selectedTags.size();
    auto it = std::remove_if(m_selectedTags.begin(), m_selectedTags.end(), [&tag](const SelectedTag& t) {
        return t.tag.compare(tag, Qt::CaseInsensitive) == 0;
    });
    m_selectedTags.erase(it, m_selectedTags.end());

    if (m_selectedTags.size() == count) {
        return; // nothing removed
    }

    invalidate();
    emit filtersChanged();
    emit summaryTextChanged();
    emit countChanged();
}

void AircraftFilterModel::toggleTagNegation(QString tag)
{
    for (auto& t : m_selectedTags) {
        if (t.tag.compare(tag, Qt::CaseInsensitive) == 0) {
            t.negated = !t.negated;
            invalidate();
            emit filtersChanged();
            emit summaryTextChanged();
            emit countChanged();
            return;
        }
    }
}

void AircraftFilterModel::clearTags()
{
    if (m_selectedTags.isEmpty()) {
        return;
    }

    m_selectedTags.clear();
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

    if (!m_selectedTags.isEmpty()) {
        const QStringList aircraftTags = index.data(AircraftTagsRole).toStringList();
        for (const auto& selected : m_selectedTags) {
            const bool present = aircraftTags.contains(selected.tag, Qt::CaseInsensitive);
            if (present == selected.negated) {
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
    auto order = Qt::AscendingOrder;
    switch (m_sorting) {
    case Sorting::SortByName:
        setSortRole(AircraftVariantDescriptionRole);
        setSortCaseSensitivity(Qt::CaseInsensitive);

        break;
    case Sorting::SortByRating:
        setSortRole(AircraftCombinedRatingRole);
        // highest-rated at the top
        order = Qt::DescendingOrder;
        break;
    }

    setDynamicSortFilter(true);
    sort(0, order);
}

bool AircraftFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (m_sorting == Sorting::SortByRating) {
        // use default impl
        return QSortFilterProxyModel::lessThan(left, right);
    }

    // custom imply for SortByName
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
