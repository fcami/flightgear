// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include <QUrl>

class AircraftItemModel;

/**
 * Proxy model that applies ratings, compatibility, and sorting to a
 * pre-filtered source model.
 *
 * The source model is swapped at runtime (via setSourceModel) depending on
 * which aircraft list tab is active (see LauncherController::setSelectedModel).
 */
class AircraftFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum class Sorting {
        SortByName = 0,
        SortByRating,
    };

    Q_ENUM(Sorting)

    AircraftFilterModel(QObject* pr, QAbstractItemModel* source);

    Q_PROPERTY(QList<int> ratings READ ratings WRITE setRatings NOTIFY filtersChanged)

    Q_PROPERTY(bool ratingsFilterEnabled READ ratingsFilterEnabled WRITE setRatingFilterEnabled NOTIFY filtersChanged)

    Q_PROPERTY(bool compatibilityFilterEnabled READ compatibilityFilterEnabled WRITE setCompatibilityFilterEnabled NOTIFY filtersChanged)

    Q_PROPERTY(bool filteringEnabled READ filteringEnabled WRITE setFilteringEnabled NOTIFY filtersChanged)

    Q_PROPERTY(QString summaryText READ summaryText NOTIFY summaryTextChanged)

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_PROPERTY(int filteredOutCount READ filteredOutCount NOTIFY filtersChanged)

    Q_PROPERTY(Sorting sorting READ sorting WRITE setSorting NOTIFY sortingChanged)

    /**
     * Compute the row (index in QML / ListView speak) based on an aircraft URI.
     * Returns -1 if the URI is not present in the (filtered) model.
     * Handles both direct AircraftItemModel sources and proxy-chained sources.
     */
    Q_INVOKABLE int indexForURI(QUrl uri) const;

    Q_INVOKABLE void selectVariantForAircraftURI(QUrl uri);

    Q_INVOKABLE void loadCompatibilityAndRatingsSettings();

    Q_INVOKABLE void saveCompatibilityAndRatingsSettings();

    QList<int> ratings() const
    {
        return m_ratings;
    }

    bool ratingsFilterEnabled() const
    {
        return m_ratingsFilter;
    }

    void setRatings(QList<int> ratings);
    void setRatingFilterEnabled(bool e);

    bool compatibilityFilterEnabled() const
    {
        return m_compatibilityFilter;
    }

    void setCompatibilityFilterEnabled(bool e);

    void setFilteringEnabled(bool e);

    bool filteringEnabled() const
    {
        return m_filteringEnabled;
    }

    Sorting sorting() const;
    void setSorting(Sorting s);

    QString summaryText() const;

    int count() const;

    int filteredOutCount() const;

signals:
    void filtersChanged();
    void summaryTextChanged();
    void countChanged();
    void sortingChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    AircraftItemModel* resolveItemModel() const;
    void updateSorting();

    bool m_ratingsFilter = false;
    bool m_compatibilityFilter = true;
    bool m_filteringEnabled = true;

    QList<int> m_ratings;
    Sorting m_sorting = Sorting::SortByName;
};
