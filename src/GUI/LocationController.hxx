/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2015 James Turner
 */


#pragma once

#include <QObjectList>

#include <Navaids/positioned.hxx>
#include <Airports/airports_fwd.hxx>

#include "CarriersLocationModel.hxx"
#include "LaunchConfig.hxx"
#include "QmlPositioned.hxx"
#include "QtLauncher_fwd.hxx"
#include "UnitsModel.hxx"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
Q_MOC_INCLUDE("NavaidSearchModel.hxx")
Q_MOC_INCLUDE("QmlPositionedModel.hxx")
Q_MOC_INCLUDE("QmlStringListModel.hxx")
#endif

class NavaidSearchModel;
class QmlPositionedModel;
class QmlStringListModel;

class LocationController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)

    Q_PROPERTY(NavaidSearchModel* searchModel MEMBER m_searchModel CONSTANT)
    Q_PROPERTY(CarriersLocationModel* carriersModel MEMBER m_carriersModel CONSTANT)

    Q_PROPERTY(QmlPositionedModel* airportRunways READ airportRunways CONSTANT)
    Q_PROPERTY(QmlPositionedModel* airportParkings READ airportParkings CONSTANT)
    Q_PROPERTY(QmlPositionedModel* airportHelipads READ airportHelipads CONSTANT)

    Q_PROPERTY(bool offsetEnabled READ offsetEnabled WRITE setOffsetEnabled NOTIFY offsetChanged)
    Q_PROPERTY(QuantityValue offsetRadial READ offsetRadial WRITE setOffsetRadial NOTIFY offsetChanged)
    Q_PROPERTY(QuantityValue offsetDistance READ offsetDistance WRITE setOffsetDistance NOTIFY offsetChanged)

    Q_PROPERTY(bool headingEnabled MEMBER m_headingEnabled NOTIFY configChanged)
    Q_PROPERTY(bool speedEnabled MEMBER m_speedEnabled NOTIFY configChanged)
    Q_PROPERTY(bool altitudeEnabled MEMBER m_altitudeEnabled NOTIFY configChanged)

    Q_PROPERTY(QuantityValue heading MEMBER m_heading NOTIFY configChanged)
    Q_PROPERTY(QuantityValue altitude MEMBER m_altitude NOTIFY configChanged)
    Q_PROPERTY(QuantityValue airspeed MEMBER m_airspeed NOTIFY configChanged)
    Q_PROPERTY(bool onFinal READ onFinal WRITE setOnFinal NOTIFY configChanged)

    Q_PROPERTY(bool isAirportLocation READ isAirportLocation NOTIFY baseLocationChanged)
    Q_PROPERTY(bool useActiveRunway READ useActiveRunway WRITE setUseActiveRunway NOTIFY configChanged)
    Q_PROPERTY(bool useAvailableParking READ useAvailableParking WRITE setUseAvailableParking NOTIFY configChanged)

    Q_PROPERTY(bool tuneNAV1 READ tuneNAV1 WRITE setTuneNAV1 NOTIFY configChanged)
    Q_PROPERTY(QmlGeod baseGeod READ baseGeod WRITE setBaseGeod NOTIFY baseLocationChanged)

    Q_PROPERTY(QmlPositioned* base READ baseLocation CONSTANT)
    Q_PROPERTY(QmlPositioned* detail READ detail CONSTANT)
    Q_PROPERTY(bool isBaseLatLon READ isBaseLatLon NOTIFY baseLocationChanged)

    Q_PROPERTY(bool isCarrier READ isCarrier NOTIFY baseLocationChanged)
    Q_PROPERTY(QString carrier READ carrierName WRITE setCarrierLocation NOTIFY baseLocationChanged)
    Q_PROPERTY(QmlStringListModel* carrierParkings READ carrierParkings CONSTANT)
    Q_PROPERTY(bool useCarrierFLOLS READ useCarrierFLOLS WRITE setUseCarrierFLOLS NOTIFY configChanged)
    Q_PROPERTY(QString carrierParking READ carrierParking WRITE setCarrierParking NOTIFY configChanged)
    Q_PROPERTY(bool abeam READ abeam WRITE setAbeam NOTIFY configChanged)


    // allow collecting the location properties to be disabled, if the
    // user is setting conflicting ones
    Q_PROPERTY(bool skipFromArgs MEMBER m_skipFromArgs NOTIFY skipFromArgsChanged)
public:
    explicit LocationController(QObject *parent = nullptr);
    ~LocationController();

    void setLaunchConfig(LaunchConfig* config);

    QString description() const;

    void setBaseLocation(FGPositionedRef ref);

    bool shouldStartPaused() const;

    void setLocationProperties();

    void restoreLocation(QVariantMap l);
    QVariantMap saveLocation() const;

    void restoreSearchHistory();

    /// used to automatically select aircraft state
    bool isParkedLocation() const;

    /// used to automatically select aircraft state
    bool isAirborneLocation() const;

    QuantityValue offsetRadial() const;

    QuantityValue offsetDistance() const
    {
        return m_offsetDistance;
    }

    Q_INVOKABLE void setBaseLocation(QmlPositioned* pos);

    Q_INVOKABLE void setDetailLocation(QmlPositioned* pos);

    QmlGeod baseGeod() const;
    void setBaseGeod(QmlGeod geod);

    QString carrierName() const;
    void setCarrierLocation(QString name);

    bool isAirportLocation() const;

    bool offsetEnabled() const
    {
        return m_offsetEnabled;
    }

    bool onFinal() const
    {
        return m_onFinal;
    }

    void setUseActiveRunway(bool b);

    bool useActiveRunway() const
    {
        return m_useActiveRunway;
    }

    Q_INVOKABLE void addToRecent(QmlPositioned* pos);

    QmlPositionedModel* airportRunways() const
    {
        return m_runwaysModel;
    }

    QmlPositionedModel* airportHelipads() const
    {
        return m_helipadsModel;
    }

    QmlPositionedModel* airportParkings() const
    {
        return m_parkingsModel;
    }

    Q_INVOKABLE void showHistoryInSearchModel();

    Q_INVOKABLE QmlGeod parseStringAsGeod(QString string) const;

    QString carrierParking() const;
    void setCarrierParking(QString name);

    bool tuneNAV1() const
    {
        return m_tuneNAV1;
    }

    QmlPositioned* detail() const;

    QmlPositioned* baseLocation() const;

    bool useAvailableParking() const
    {
        return m_useAvailableParking;
    }

    bool isBaseLatLon() const
    {
        return m_locationIsLatLon;
    }

    QuantityValue altitude() const
    {
        return m_altitude;
    }

    bool isCarrier() const
    {
        return m_locationIsCarrier;
    }

    QmlStringListModel* carrierParkings() const;

    bool useCarrierFLOLS() const
    {
        return m_useCarrierFLOLS;
    }

    bool abeam() const
    {
        return m_abeam;
    }

public slots:
    void setOffsetRadial(QuantityValue offsetRadial);

    void setOffsetDistance(QuantityValue offsetNm);

    void setOffsetEnabled(bool offsetEnabled);

    void setOnFinal(bool onFinal);

    void setTuneNAV1(bool tuneNAV1);

    void setUseAvailableParking(bool useAvailableParking);

    void setUseCarrierFLOLS(bool useCarrierFLOLS);

    void setAbeam(bool abeam);

Q_SIGNALS:
    void descriptionChanged();
    void offsetChanged();
    void baseLocationChanged();
    void configChanged();
    void skipFromArgsChanged();

private Q_SLOTS:
    void onCollectConfig();
    void onRestoreCurrentLocation();
    void onSaveCurrentLocation();
private:
    void clearLocation();
    void updateAirportModels();

    void onSearchComplete();
    void addToRecent(FGPositionedRef pos);
    void setNavRadioOption();

    void applyPositionOffset();
    void applyAltitude();
    void applyAirspeed();
    void applyOnFinal();

    QString compassPointFromHeading(int heading) const;

    NavaidSearchModel* m_searchModel = nullptr;
    CarriersLocationModel* m_carriersModel = nullptr;

    FGPositionedRef m_location;
    FGAirportRef m_airportLocation; // valid if m_location is an FGAirport
    FGPositionedRef m_detailLocation; // parking stand or runway detail
    bool m_locationIsLatLon = false;
    SGGeod m_geodLocation;
    bool m_locationIsCarrier = false;
    QString m_carrierName;

    FGPositionedList m_recentLocations;
    LaunchConfig* m_config = nullptr;
    QmlPositioned* m_detailQml = nullptr;
    QmlPositioned* m_baseQml = nullptr;

    QmlPositionedModel* m_runwaysModel;
    QmlPositionedModel* m_helipadsModel;
    QmlPositionedModel* m_parkingsModel;

    bool m_offsetEnabled = false;
    QuantityValue m_offsetRadial;
    QuantityValue m_offsetDistance;
    QuantityValue m_heading;
    QuantityValue m_altitude;
    QuantityValue m_airspeed;

    QuantityValue m_defaultAirspeed, m_defaultAltitude,
        m_defaultHeading, m_defaultOffsetDistance, m_defaultOffsetRadial;

    bool m_onFinal = false;
    bool m_useActiveRunway = true;
    bool m_tuneNAV1 = false;
    bool m_useAvailableParking;
    bool m_headingEnabled = false;
    bool m_speedEnabled = false;
    bool m_altitudeEnabled = false;
    bool m_skipFromArgs = false;
    bool m_abeam;

    bool m_useCarrierFLOLS = false;
    QString m_carrierParking;
    QmlStringListModel* m_carrierParkingsModel = nullptr;
};
