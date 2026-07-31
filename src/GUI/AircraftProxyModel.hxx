// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef AIRCRAFTSEARCHFILTERMODEL_HXX
#define AIRCRAFTSEARCHFILTERMODEL_HXX

#include <QSortFilterProxyModel>
#include <QUrl>

#include <simgear/props/props.hxx>

class AircraftProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    AircraftProxyModel(QObject* pr, QAbstractItemModel * source);

    enum SortMode {
        SortByRating = 0,   //< overall (community) rating, highest first
        SortByName,         //< aircraft name, A-Z
    };
    Q_ENUM(SortMode)

    Q_PROPERTY(QList<int> ratings READ ratings WRITE setRatings NOTIFY ratingsChanged)

    Q_PROPERTY(bool ratingsFilterEnabled READ ratingsFilterEnabled WRITE setRatingFilterEnabled NOTIFY ratingsFilterEnabledChanged)

    Q_PROPERTY(bool compatibilityFilterEnabled READ compatibilityFilterEnabled WRITE setCompatibilityFilterEnabled NOTIFY compatibilityFilterEnabledChanged)

    Q_PROPERTY(QString summaryText READ summaryText NOTIFY summaryTextChanged)

    Q_INVOKABLE void setAircraftFilterString(QString s);

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)

    /**
      * Compute the row (index in QML / ListView speak) based on an aircraft URI.
      * Return -1 if the UIR is not present in the (filtered) model
      **/
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

    QString summaryText() const;

    int count() const;

    int sortMode() const
    {
        return m_sortMode;
    }

    void setSortMode(int mode);
signals:
    void ratingsChanged();
    void ratingsFilterEnabledChanged();
    void compatibilityFilterEnabledChanged();
    void summaryTextChanged();
    void countChanged();
    void sortModeChanged();

public slots:

    void setInstalledFilterEnabled(bool e);

    void setHaveUpdateFilterEnabled(bool e);

    void setShowFavourites(bool e);
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    bool filterAircraft(const QModelIndex& sourceIndex) const;

    bool m_ratingsFilter = false;
    bool m_compatibilityFilter = true;
    bool m_onlyShowInstalled = false;
    bool m_onlyShowWithUpdate = false;
    bool m_onlyShowFavourites = false;
    int m_sortMode = SortByRating;

    QList<int> m_ratings;
    QString m_filterString;
    SGPropertyNode_ptr m_filterProps;
};

#endif // AIRCRAFTSEARCHFILTERMODEL_HXX
