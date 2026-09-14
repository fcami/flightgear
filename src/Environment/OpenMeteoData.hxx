/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cstddef>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include <simgear/math/SGMath.hxx>

namespace Environment {

class AtmosphericProfile;

struct OpenMeteoCloudLayer {
    double baseAltitudeFt = 0.0;
    double topAltitudeFt = 0.0;
    double coveragePercent = 0.0;
    double densityNorm = 0.0;
    double turbulenceNorm = 0.0;
    std::string coverageType = "clear";
};

struct OpenMeteoSurfaceData {
    double seaLevelPressureHpa = 1013.25;
    double surfacePressureHpa = 1013.25;
    double temperatureC = 15.0;
    double dewPointC = 10.0;
    double relativeHumidityPercent = 70.0;
    double visibilityM = 32000.0;
    double windDirectionDeg = 0.0;
    double windSpeedKt = 0.0;
    double windGustKt = 0.0;
    double precipitationMmPerHour = 0.0;
    double rainMmPerHour = 0.0;
    double showersMmPerHour = 0.0;
    double snowfallCmPerHour = 0.0;
    double cloudCoverPercent = 0.0;
    double cloudCoverLowPercent = 0.0;
    double cloudCoverMidPercent = 0.0;
    double cloudCoverHighPercent = 0.0;
    double weatherCode = 0.0;
    double freezingLevelHeightM = 2500.0;
    double capeJkg = 0.0;
    double liftedIndex = 5.0;
    double convectiveInhibitionJkg = 0.0;
    double maxVerticalVelocityMps = 0.0;
    double convectiveSeverityNorm = 0.0;
    double icingSeverityNorm = 0.0;
    double rainNorm = 0.0;
    double snowNorm = 0.0;
    bool thunderstormLikely = false;
};

class OpenMeteoData
{
public:
    static std::unique_ptr<OpenMeteoData> parse(const std::string& payload);

    bool empty() const;

    AtmosphericProfile buildBoundaryProfile(std::time_t simTimeUtc) const;
    AtmosphericProfile buildAloftProfile(std::time_t simTimeUtc) const;
    OpenMeteoSurfaceData surfaceData(std::time_t simTimeUtc) const;
    std::vector<OpenMeteoCloudLayer> buildCloudLayers(std::time_t simTimeUtc,
                                                      std::size_t maxLayers) const;

    double seaLevelPressureHpa(std::time_t simTimeUtc) const;
    const SGGeod& dataLocation() const { return _dataLocation; }

private:
    struct PressureLevelData {
        int pressureLevelHpa = 0;
        std::vector<double> temperatureC;
        std::vector<double> dewPointC;
        std::vector<double> relativeHumidityPercent;
        std::vector<double> cloudCoverPercent;
        std::vector<double> windSpeedKt;
        std::vector<double> windDirectionDeg;
        std::vector<double> geopotentialHeightM;
        std::vector<double> verticalVelocityMps;
    };

    bool timeBracket(std::time_t simTimeUtc,
                     std::size_t& lowerIndex,
                     std::size_t& upperIndex,
                     double& alpha) const;

    double interpolateValue(const std::vector<double>& series,
                            std::time_t simTimeUtc,
                            double fallback = 0.0) const;

    double interpolateDirection(const std::vector<double>& series,
                                std::time_t simTimeUtc,
                                double fallback = 0.0) const;

    double maxAbsVerticalVelocity(std::time_t simTimeUtc) const;

    static bool parseTimestampUtc(const std::string& stamp, std::time_t& outUtc);
    static double fallbackAltitudeFt(int pressureLevelHpa);
    static double dewPointFromTemperatureAndHumidity(double temperatureC,
                                                     double relativeHumidityPercent);
    static double relativeHumidityFromTemperatureAndDewPoint(double temperatureC,
                                                             double dewPointC);
    static double convectiveSeverity(double capeJkg,
                                     double liftedIndex,
                                     double convectiveInhibitionJkg,
                                     double maxVerticalVelocityMps,
                                     bool thunderstormCode);
    static double icingSeverity(double temperatureC,
                                double relativeHumidityPercent,
                                double cloudCoverPercent,
                                double precipitationRateMmPerHour);
    static bool isThunderstormCode(int weatherCode);
    static std::string coverageTypeForPercent(double coveragePercent, double baseAltitudeFt);

    std::vector<std::time_t> _hourlyTimes;

    std::vector<double> _temperature2mC;
    std::vector<double> _dewPoint2mC;
    std::vector<double> _relativeHumidity2mPercent;
    std::vector<double> _pressureMslHpa;
    std::vector<double> _surfacePressureHpa;
    std::vector<double> _visibilityM;
    std::vector<double> _precipitationMm;
    std::vector<double> _rainMm;
    std::vector<double> _showersMm;
    std::vector<double> _snowfallCm;
    std::vector<double> _weatherCode;
    std::vector<double> _cloudCoverPercent;
    std::vector<double> _cloudCoverLowPercent;
    std::vector<double> _cloudCoverMidPercent;
    std::vector<double> _cloudCoverHighPercent;
    std::vector<double> _windSpeed10mKt;
    std::vector<double> _windDirection10mDeg;
    std::vector<double> _windGust10mKt;
    std::vector<double> _windSpeed80mKt;
    std::vector<double> _windDirection80mDeg;
    std::vector<double> _capeJkg;
    std::vector<double> _liftedIndex;
    std::vector<double> _convectiveInhibitionJkg;
    std::vector<double> _freezingLevelHeightM;

    std::vector<PressureLevelData> _pressureLevels;

    SGGeod _dataLocation;
};

} // namespace Environment

