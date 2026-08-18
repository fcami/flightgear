// SPDX-FileCopyrightText: 2015 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FG_GUI_AIRCRAFT_MODEL
#define FG_GUI_AIRCRAFT_MODEL

#include <QAbstractListModel>
#include <QDir>
#include <QPixmap>
#include <QStringList>
#include <QUrl>

#include "LocalAircraftCache.hxx"

#include <simgear/package/Delegate.hxx>
#include <simgear/package/Root.hxx>
#include <simgear/package/Catalog.hxx>
#include <simgear/package/Package.hxx>

const int AircraftPathRole = Qt::UserRole + 1;
const int AircraftAuthorsRole = Qt::UserRole + 2;
const int AircraftVariantRole = Qt::UserRole + 3;
const int AircraftVariantCountRole = Qt::UserRole + 4;
const int AircraftPackageIdRole = Qt::UserRole + 6;
const int AircraftPackageStatusRole = Qt::UserRole + 7;
const int AircraftPackageProgressRole = Qt::UserRole + 8;
const int AircraftLongDescriptionRole = Qt::UserRole + 9;
const int AircraftHasRatingsRole = Qt::UserRole + 10;
const int AircraftInstallPercentRole = Qt::UserRole + 11;
const int AircraftPackageSizeRole = Qt::UserRole + 12;
const int AircraftInstallDownloadedSizeRole = Qt::UserRole + 13;
const int AircraftURIRole = Qt::UserRole + 14;
const int AircraftIsHelicopterRole = Qt::UserRole + 16;
const int AircraftIsSeaplaneRole = Qt::UserRole + 17;
const int AircraftPackageRefRole = Qt::UserRole + 19;
const int AircraftIsFavouriteRole = Qt::UserRole + 20;
const int AircraftPrimaryURIRole = Qt::UserRole + 21;

const int AircraftStatusRole = Qt::UserRole + 22;
const int AircraftCompatibleRole = Qt::UserRole + 23;
const int AircraftTagsRole = Qt::UserRole + 24;

const int AircraftRatingRole = Qt::UserRole + 100;
const int AircraftCombinedRatingRole = Qt::UserRole + 150;
const int AircraftUserRatingAverageRole = Qt::UserRole + 151;
const int AircraftUserRatingVotesRole = Qt::UserRole + 152;
const int AircraftUserOwnRatingRole = Qt::UserRole + 153; ///< -1 if not rated by the user
const int AircraftUserCanRateRole = Qt::UserRole + 154;   ///< installed, flown enough, not yet rated
const int AircraftVariantDescriptionRole = Qt::UserRole + 200;

class PackageDelegate;

Q_DECLARE_METATYPE(simgear::pkg::PackageRef)

class AircraftItemModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int installedAircraftCount READ installedAircraftCount NOTIFY installedAircraftCountChanged)

public:

    AircraftItemModel(QObject* pr);

    ~AircraftItemModel() override;

    void setPackageRoot(const simgear::pkg::RootRef& root);

    int rowCount(const QModelIndex& parent) const override;

    QVariant data(const QModelIndex& index, int role) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    QHash<int, QByteArray> roleNames() const override;

    /**
     * given a -set.xml path, return the corresponding model index, if one
     * exists.
     */

    QModelIndex indexOfAircraftURI(QUrl uri) const;

    /**
     * ensure the appropriate variant index is active in the model, for the
     * corresponding aircraft URI
     */
    void selectVariantForAircraftURI(QUrl uri);

    /**
     * return if a given aircraft is ready to be run, or not. Aircraft which
     * are not installed, or are downloading, are not runnable.
     */
    bool isIndexRunnable(const QModelIndex& index) const;

    /**
     * Retrieve the display name for an aircraft specified by URI, without
     * changing the current variant state
     */
    QString nameForAircraftURI(QUrl uri) const;

    int installedAircraftCount() const;
signals:
    void aircraftInstallFailed(QModelIndex index, QString errorMessage);

    void aircraftInstallCompleted(QModelIndex index);

    void contentsChanged();

    void installedAircraftCountChanged();
private slots:
    void onScanStarted();
    void onScanAddedItems(int count);
    void onLocalCacheCleared();

private:
    friend class PackageDelegate;

    /**
     * struct to record persistent state from the item-delegate, about the
     * currently visible variant, thumbnail and similar.
     */
    struct DelegateState
    {
        quint32 variant = 0;
    };

    QVariant dataFromItem(AircraftItemPtr item, const DelegateState& state, int role) const;

    QVariant dataFromPackage(const simgear::pkg::PackageRef& item,
                             const DelegateState& state, int role) const;

    QVariant packageRating(const simgear::pkg::PackageRef& p, int ratingIndex) const;
    QVariant packageThumbnail(simgear::pkg::PackageRef p,
                              const DelegateState& state, bool download = true) const;

    QVariant packagePreviews(simgear::pkg::PackageRef p, const DelegateState &ds) const;

    void refreshPackages();

    void installSucceeded(QModelIndex index);
    void installFailed(QModelIndex index, simgear::pkg::Delegate::StatusCode reason);

private:
    PackageDelegate* m_delegate = nullptr;

    QVector<DelegateState> m_delegateStates;

    simgear::pkg::RootRef m_packageRoot;
    simgear::pkg::PackageList m_packages;
    int m_cachedLocalAircraftCount = 0;

};

#endif // of FG_GUI_AIRCRAFT_MODEL
