// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include <QUrl>
#include <QVariantList>

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
     * List of selected tags to filter by. Each entry is a map with "tag"
     * (string) and "negated" (bool) keys: a negated entry requires the tag
     * to be *absent* rather than present. Aircraft must satisfy every entry
     * (logical AND) to pass the filter.
     */
    Q_PROPERTY(QVariantList selectedTags READ selectedTags WRITE setSelectedTags NOTIFY filtersChanged)

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

    QVariantList selectedTags() const;
    void setSelectedTags(QVariantList tags);

    /// true if \p tag is currently selected (negated or not)
    Q_INVOKABLE bool hasTag(QString tag) const;

    /// add \p tag to the selection (non-negated) if not already present
    Q_INVOKABLE void addTag(QString tag);

    /// remove \p tag from the selection, if present
    Q_INVOKABLE void removeTag(QString tag);

    /// flip the negated state of \p tag, if present
    Q_INVOKABLE void toggleTagNegation(QString tag);

    Q_INVOKABLE void clearTags();

signals:
    void filtersChanged();
    void summaryTextChanged();
    void countChanged();
    void sortingChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    struct SelectedTag {
        QString tag;
        bool negated = false;
    };

    AircraftItemModel* resolveItemModel() const;
    void updateSorting();

    bool m_ratingsFilter = false;
    bool m_compatibilityFilter = true;
    bool m_filteringEnabled = true;

    QList<int> m_ratings;
    Sorting m_sorting = Sorting::SortByName;
    QList<SelectedTag> m_selectedTags;
};
