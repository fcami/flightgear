// LauncherController.hxx - GUI launcher dialog using Qt5
//
// SPDX-FileCopyrightText: 2018 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QUrl>
#include <QModelIndex>
#include <QSize>
#include <QPointF>

#include <simgear/package/Package.hxx>
#include <simgear/package/Catalog.hxx>

#include "AircraftFavouritesModel.hxx"
#include "AircraftFilterModel.hxx"
#include "AircraftInstalledModel.hxx"
#include "AircraftItemModel.hxx"
#include "AircraftSearchModel.hxx"
#include "AircraftUpdatesModel.hxx"
#include "FlightPlanController.hxx"
#include "LocationController.hxx"
#include "MPServersModel.h"
#include "QmlAircraftInfo.hxx"
#include "RecentAircraftModel.hxx"
#include "RecentLocationsModel.hxx"

// forward decls
class QTimer;
class QWindow;
class QQuickItem;
class LaunchConfig;

class LauncherController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AircraftFilterModel* currentAircraftModel MEMBER m_aircraftFilterModel CONSTANT)
    Q_PROPERTY(AircraftUpdatesModel* aircraftWithUpdatesModel MEMBER m_aircraftWithUpdatesModel CONSTANT)
    Q_PROPERTY(AircraftSearchModel* aircraftSearchModel MEMBER m_aircraftSearchModel CONSTANT)
    Q_PROPERTY(AircraftFavouritesModel* favouriteAircraftModel MEMBER m_favouriteAircraftModel CONSTANT)

    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged)

    Q_PROPERTY(AircraftItemModel* baseAircraftModel MEMBER m_aircraftModel CONSTANT)

    Q_PROPERTY(LocationController* location MEMBER m_location CONSTANT)
    Q_PROPERTY(FlightPlanController* flightPlan MEMBER m_flightPlan CONSTANT)

    Q_PROPERTY(MPServersModel* mpServersModel MEMBER m_serversModel CONSTANT)

    Q_PROPERTY(QUrl selectedAircraft READ selectedAircraft WRITE setSelectedAircraft NOTIFY selectedAircraftChanged)

    Q_PROPERTY(QmlAircraftInfo* selectedAircraftInfo READ selectedAircraftInfo NOTIFY selectedAircraftChanged)

    Q_PROPERTY(QString selectedAircraftState MEMBER m_aircraftState NOTIFY selectedAircraftStateChanged)

    Q_PROPERTY(bool isSearchActive READ isSearchActive NOTIFY searchChanged)
    Q_PROPERTY(QString settingsSearchTerm READ settingsSearchTerm WRITE setSettingsSearchTerm NOTIFY searchChanged)

    Q_PROPERTY(QStringList settingsSummary READ settingsSummary WRITE setSettingsSummary NOTIFY summaryChanged)
    Q_PROPERTY(QStringList environmentSummary READ environmentSummary WRITE setEnvironmentSummary NOTIFY summaryChanged)

    Q_PROPERTY(QStringList combinedSummary READ combinedSummary NOTIFY summaryChanged)

    Q_PROPERTY(QString versionString READ versionString CONSTANT)

    Q_PROPERTY(RecentAircraftModel* aircraftHistory READ aircraftHistory CONSTANT)
    Q_PROPERTY(RecentLocationsModel* locationHistory READ locationHistory CONSTANT)

    Q_PROPERTY(bool canFly READ canFly NOTIFY canFlyChanged)

    Q_PROPERTY(AircraftType aircraftType READ aircraftType NOTIFY selectedAircraftChanged)

	Q_PROPERTY(QSize minimumWindowSize READ minWindowSize WRITE setMinWindowSize NOTIFY minWindowSizeChanged)

    Q_PROPERTY(QUrl flyIconUrl READ flyIconUrl NOTIFY selectedAircraftChanged)
    Q_PROPERTY(QUrl flyButtonLabel READ flyButtonLabel NOTIFY selectedAircraftChanged)

    Q_PROPERTY(bool inAppMode READ inApp NOTIFY inAppChanged)

    Q_PROPERTY(bool aircraftGridMode READ aircraftGridMode WRITE setAircraftGridMode NOTIFY aircraftGridModeChanged)

    Q_PROPERTY(int launchCount READ launchCount CONSTANT);
    Q_PROPERTY(int versionLaunchCount READ versionLaunchCount CONSTANT);

    // property to indicate if we're disabling the aircraft selection because the user entered it manually
    Q_PROPERTY(bool skipAircraftFromArgs MEMBER m_skipAircraftFromArgs NOTIFY skipAircraftFromArgsChanged)

    Q_PROPERTY(bool isNetworkAvailable READ isNetworkAvailable NOTIFY networkAvailableChanged)
public:
    explicit LauncherController(QObject *parent, QWindow* win);

    void initQML(int& styleTypeId);

    Q_INVOKABLE bool validateMetarString(QString metar);

    Q_INVOKABLE void requestInstallUpdate(QUrl aircraftUri);

    Q_INVOKABLE void requestUninstall(QUrl aircraftUri);

    Q_INVOKABLE void requestInstallCancel(QUrl aircraftUri);

    Q_INVOKABLE void downloadDirChanged(QString path);

    Q_INVOKABLE void requestUpdateAllAircraft();

    Q_INVOKABLE void queryMPServers();

    QUrl selectedAircraft() const;

    // work around the fact, that this is not available on QQuickItem until 5.7
    Q_INVOKABLE QPointF mapToGlobal(QQuickItem* item, const QPointF& pos) const;

    QmlAircraftInfo* selectedAircraftInfo() const;

    Q_INVOKABLE void restoreLocation(QVariant var);

    Q_INVOKABLE bool matchesSearch(QString term, QStringList keywords) const;

    bool isSearchActive() const;

    QString settingsSearchTerm() const
    {
        return m_settingsSearchTerm;
    }

    QString selectedModel() const
    {
        return m_selectedModel;
    }

    QStringList settingsSummary() const;

    QStringList environmentSummary() const;

    QStringList combinedSummary() const;

    QString versionString() const;

    RecentAircraftModel* aircraftHistory();

    RecentLocationsModel* locationHistory();

    Q_INVOKABLE void launchUrl(QUrl url);

    // list of QUrls containing the default splash images from FGData.
    // used on the summary screen
    Q_INVOKABLE QVariantList defaultSplashUrls() const;

    Q_INVOKABLE QVariant loadUISetting(QString name, QVariant defaultValue) const;
    Q_INVOKABLE void saveUISetting(QString name, QVariant value) const;

    /**
     * @brief urlToDataPath - convert a FGData path into a global file:/// URL suitable for Qt.openExternally()
     * @param relPath - the path rlative to FG_ROOT
     * @return a file QUrl
     */
    Q_INVOKABLE QUrl urlToDataPath(QString relPath) const;

    LaunchConfig* config() const
    { return m_config; }

    void doRun();
    void doApply();

    bool canFly() const;

    AircraftItemModel* baseAircraftModel() const
    { return m_aircraftModel; }

    void initialRestoreSettings();
    void saveSettings();

    LocationController* location() const
    { return m_location; }

    enum AircraftType
    {
        Unknown = 0,
        Airplane,
        Seaplane,
        Helicopter,
        Airship
    };

    Q_ENUMS(AircraftType)

    AircraftType aircraftType() const
    { return m_aircraftType; }

	void setInAppMode();
	bool keepRunningInAppMode() const;
	bool inAppResult() const;

    QSize minWindowSize() const
    {
        return m_minWindowSize;
    }

    void setMinWindowSize(QSize sz);

    QUrl flyIconUrl() const;

    bool aircraftGridMode() const
    {
        return m_aircraftGridMode;
    }

    bool  inApp() const
    {
        return m_inAppMode;
    }

    int launchCount() const
    {
        return m_launchCount;
    }

    int versionLaunchCount() const
    {
        return m_versionLaunchCount;
    }

    QString flyButtonLabel() const;

    bool isNetworkAvailable() const;
signals:

    void selectedAircraftChanged(QUrl selectedAircraft);
    void selectedAircraftStateChanged();

    void searchChanged();
    void summaryChanged();
	void minWindowSizeChanged();
    void canFlyChanged();

    void viewCommandLine();

    void aircraftGridModeChanged(bool aircraftGridMode);

    void inAppChanged();
    void installedAircraftCountChanged(int installedAircraftCount);

    void didResetGettingStartedTips();
    void skipAircraftFromArgsChanged();

    void networkAvailableChanged();

    void selectedModelChanged();

public slots:
    void setSelectedAircraft(QUrl selectedAircraft);

    void setSelectedModel(const QString& state);

    void setSettingsSearchTerm(QString settingsSearchTerm);

    void setSettingsSummary(QStringList settingsSummary);

    void setEnvironmentSummary(QStringList environmentSummary);

    void fly();
	void quit();

	void requestRestoreDefaults();

	void requestChangeDataPath();

    void openConfig();
    void saveConfigAs();
    void setAircraftGridMode(bool aircraftGridMode);

    void resetGettingStartedTips();

private slots:

    void onAircraftInstalledCompleted(QModelIndex index);
    void onAircraftInstallFailed(QModelIndex index, QString errorMessage);

    void saveAircraft();
    void restoreAircraft();

    void checkForOldDownloadDir();

private:
    /**
     * Check if the passed index is the selected aircraft, and if so, refresh
     * the associated UI data
     */
    void maybeUpdateSelectedAircraft(QModelIndex index);
    void updateSelectedAircraft();


    simgear::pkg::PackageRef packageForAircraftURI(QUrl uri) const;

    // need to wait after a model reset before restoring selection and
    // scrolling, to give the view time it seems.
    void delayedAircraftModelReset();

    void collectAircraftArgs();

    QString selectAircraftStateAutomatically();
    bool haveOldWindowsDownloadDir() const;

    void onReachabilityChanged();
    void delayedPackageRefresh();

private:
    QWindow* m_window = nullptr;

    AircraftFilterModel* m_aircraftFilterModel = nullptr;
    AircraftInstalledModel* m_aircraftInstalledModel = nullptr;
    AircraftItemModel* m_aircraftModel;
    AircraftSearchModel* m_aircraftSearchModel;
    AircraftUpdatesModel* m_aircraftWithUpdatesModel;
    AircraftFavouritesModel* m_favouriteAircraftModel;

    MPServersModel* m_serversModel = nullptr;
    LocationController* m_location = nullptr;
    FlightPlanController* m_flightPlan = nullptr;

    QUrl m_selectedAircraft;
    QString m_aircraftState;
    AircraftType m_aircraftType = Airplane;
    int m_ratingFilters[4] = {3, 3, 3, 3};
    LaunchConfig* m_config = nullptr;
    QmlAircraftInfo* m_selectedAircraftInfo = nullptr;
    QString m_settingsSearchTerm;
    QStringList m_settingsSummary, m_environmentSummary;
    RecentAircraftModel* m_aircraftHistory = nullptr;
    RecentLocationsModel* m_locationHistory = nullptr;
    QString m_selectedModel;
    QSize m_minWindowSize;
    QTimer* m_subsystemIdleTimer = nullptr;

	bool m_inAppMode = false;
	bool m_keepRunningInAppMode = false;
    bool m_appModeResult = true;
    bool m_aircraftGridMode;
    int m_launchCount = 0;
    int m_versionLaunchCount = 0;
    bool m_flyRequested = false;
    bool m_skipAircraftFromArgs = false;
};
