/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "OpenMeteoData.hxx"

#include "AtmosphericProfile.hxx"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <simgear/constants.h>
#include <simgear/math/SGMath.hxx>
#include <simgear/timing/sg_time.hxx>

namespace {

constexpr int PRESSURE_LEVELS[] = {
    1000, 975, 950, 925, 900, 850, 800, 750, 700, 650,
    600, 550, 500, 450, 400, 350, 300, 250, 200
};

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();

bool readDoubleArrayRequired(const nlohmann::json& hourly,
                             const std::string& key,
                             std::size_t expectedSize,
                             std::vector<double>& outValues)
{
    const auto it = hourly.find(key);
    if ((it == hourly.end()) || !it->is_array() || (it->size() != expectedSize)) {
        return false;
    }

    outValues.clear();
    outValues.reserve(expectedSize);
    for (const auto& value : *it) {
        if (!value.is_number()) {
            return false;
        }

        outValues.push_back(value.get<double>());
    }

    return true;
}

void readDoubleArrayOptional(const nlohmann::json& hourly,
                             const std::string& key,
                             std::size_t expectedSize,
                             std::vector<double>& outValues,
                             double defaultValue = NAN_VALUE)
{
    outValues.assign(expectedSize, defaultValue);

    const auto it = hourly.find(key);
    if ((it == hourly.end()) || !it->is_array() || (it->size() != expectedSize)) {
        return;
    }

    for (std::size_t i = 0; i < expectedSize; ++i) {
        const auto& value = (*it)[i];
        if (value.is_number()) {
            outValues[i] = value.get<double>();
        }
    }
}

double absDirectionDifference(double aDeg, double bDeg)
{
    return std::fabs(SGMiscd::normalizePeriodic(-180.0, 180.0, bDeg - aDeg));
}

double coverageScore(double coveragePercent)
{
    return SGMiscd::clip(coveragePercent / 100.0, 0.0, 1.0);
}

double humidityScore(double relativeHumidityPercent)
{
    return SGMiscd::clip((relativeHumidityPercent - 60.0) / 40.0, 0.0, 1.0);
}

} // namespace

namespace Environment {

std::unique_ptr<OpenMeteoData> OpenMeteoData::parse(const std::string& payload)
{
    const auto root = nlohmann::json::parse(payload, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return {};
    }

    const auto hourlyIt = root.find("hourly");
    if ((hourlyIt == root.end()) || !hourlyIt->is_object()) {
        return {};
    }

    const auto& hourly = *hourlyIt;
    const auto timeIt = hourly.find("time");
    if ((timeIt == hourly.end()) || !timeIt->is_array()) {
        return {};
    }

    auto data = std::unique_ptr<OpenMeteoData>(new OpenMeteoData);
    data->_hourlyTimes.reserve(timeIt->size());
    for (const auto& t : *timeIt) {
        if (!t.is_string()) {
            return {};
        }

        std::time_t parsedTime = 0;
        if (!parseTimestampUtc(t.get<std::string>(), parsedTime)) {
            return {};
        }

        data->_hourlyTimes.push_back(parsedTime);
    }

    if (data->_hourlyTimes.empty()) {
        return {};
    }

    const std::size_t hourlyCount = data->_hourlyTimes.size();
    if (!readDoubleArrayRequired(hourly, "temperature_2m", hourlyCount, data->_temperature2mC)) {
        return {};
    }
    if (!readDoubleArrayRequired(hourly, "pressure_msl", hourlyCount, data->_pressureMslHpa)) {
        return {};
    }
    if (!readDoubleArrayRequired(hourly, "wind_speed_10m", hourlyCount, data->_windSpeed10mKt)) {
        return {};
    }
    if (!readDoubleArrayRequired(hourly, "wind_direction_10m", hourlyCount, data->_windDirection10mDeg)) {
        return {};
    }

    readDoubleArrayOptional(hourly, "surface_pressure", hourlyCount, data->_surfacePressureHpa, NAN_VALUE);
    readDoubleArrayOptional(hourly, "dew_point_2m", hourlyCount, data->_dewPoint2mC, NAN_VALUE);
    readDoubleArrayOptional(hourly, "relative_humidity_2m", hourlyCount, data->_relativeHumidity2mPercent, NAN_VALUE);
    readDoubleArrayOptional(hourly, "visibility", hourlyCount, data->_visibilityM, NAN_VALUE);
    readDoubleArrayOptional(hourly, "precipitation", hourlyCount, data->_precipitationMm, 0.0);
    readDoubleArrayOptional(hourly, "rain", hourlyCount, data->_rainMm, 0.0);
    readDoubleArrayOptional(hourly, "showers", hourlyCount, data->_showersMm, 0.0);
    readDoubleArrayOptional(hourly, "snowfall", hourlyCount, data->_snowfallCm, 0.0);
    readDoubleArrayOptional(hourly, "weather_code", hourlyCount, data->_weatherCode, 0.0);
    readDoubleArrayOptional(hourly, "cloud_cover", hourlyCount, data->_cloudCoverPercent, NAN_VALUE);
    readDoubleArrayOptional(hourly, "cloud_cover_low", hourlyCount, data->_cloudCoverLowPercent, NAN_VALUE);
    readDoubleArrayOptional(hourly, "cloud_cover_mid", hourlyCount, data->_cloudCoverMidPercent, NAN_VALUE);
    readDoubleArrayOptional(hourly, "cloud_cover_high", hourlyCount, data->_cloudCoverHighPercent, NAN_VALUE);
    readDoubleArrayOptional(hourly, "wind_gusts_10m", hourlyCount, data->_windGust10mKt, NAN_VALUE);
    readDoubleArrayOptional(hourly, "wind_speed_80m", hourlyCount, data->_windSpeed80mKt, NAN_VALUE);
    readDoubleArrayOptional(hourly, "wind_direction_80m", hourlyCount, data->_windDirection80mDeg, NAN_VALUE);
    readDoubleArrayOptional(hourly, "cape", hourlyCount, data->_capeJkg, NAN_VALUE);
    readDoubleArrayOptional(hourly, "lifted_index", hourlyCount, data->_liftedIndex, NAN_VALUE);
    readDoubleArrayOptional(hourly, "convective_inhibition", hourlyCount, data->_convectiveInhibitionJkg, NAN_VALUE);
    readDoubleArrayOptional(hourly, "freezing_level_height", hourlyCount, data->_freezingLevelHeightM, NAN_VALUE);

    for (std::size_t i = 0; i < hourlyCount; ++i) {
        if (!std::isfinite(data->_windSpeed80mKt[i])) {
            data->_windSpeed80mKt[i] = data->_windSpeed10mKt[i];
        }

        if (!std::isfinite(data->_windDirection80mDeg[i])) {
            data->_windDirection80mDeg[i] = data->_windDirection10mDeg[i];
        }

        if (!std::isfinite(data->_dewPoint2mC[i])) {
            data->_dewPoint2mC[i] = dewPointFromTemperatureAndHumidity(
                data->_temperature2mC[i], data->_relativeHumidity2mPercent[i]);
        }

        if (!std::isfinite(data->_relativeHumidity2mPercent[i])) {
            data->_relativeHumidity2mPercent[i] = relativeHumidityFromTemperatureAndDewPoint(
                data->_temperature2mC[i], data->_dewPoint2mC[i]);
        }
    }

    for (int pressureLevelHpa : PRESSURE_LEVELS) {
        const std::string levelSuffix = std::to_string(pressureLevelHpa) + "hPa";

        PressureLevelData levelData;
        levelData.pressureLevelHpa = pressureLevelHpa;
        const bool hasTemperature = readDoubleArrayRequired(hourly, "temperature_" + levelSuffix,
                                                            hourlyCount, levelData.temperatureC);
        const bool hasWindSpeed = readDoubleArrayRequired(hourly, "wind_speed_" + levelSuffix,
                                                          hourlyCount, levelData.windSpeedKt);
        const bool hasWindDirection = readDoubleArrayRequired(hourly, "wind_direction_" + levelSuffix,
                                                              hourlyCount, levelData.windDirectionDeg);
        const bool hasGeopotential = readDoubleArrayRequired(hourly, "geopotential_height_" + levelSuffix,
                                                             hourlyCount, levelData.geopotentialHeightM);

        if (!(hasTemperature && hasWindSpeed && hasWindDirection && hasGeopotential)) {
            continue;
        }

        readDoubleArrayOptional(hourly, "dew_point_" + levelSuffix, hourlyCount, levelData.dewPointC, NAN_VALUE);
        readDoubleArrayOptional(hourly, "relative_humidity_" + levelSuffix, hourlyCount,
                                levelData.relativeHumidityPercent, NAN_VALUE);
        readDoubleArrayOptional(hourly, "cloud_cover_" + levelSuffix, hourlyCount,
                                levelData.cloudCoverPercent, NAN_VALUE);
        readDoubleArrayOptional(hourly, "vertical_velocity_" + levelSuffix, hourlyCount,
                                levelData.verticalVelocityMps, 0.0);

        for (std::size_t i = 0; i < hourlyCount; ++i) {
            if (!std::isfinite(levelData.dewPointC[i])) {
                levelData.dewPointC[i] = dewPointFromTemperatureAndHumidity(
                    levelData.temperatureC[i], levelData.relativeHumidityPercent[i]);
            }

            if (!std::isfinite(levelData.relativeHumidityPercent[i])) {
                levelData.relativeHumidityPercent[i] = relativeHumidityFromTemperatureAndDewPoint(
                    levelData.temperatureC[i], levelData.dewPointC[i]);
            }
        }

        data->_pressureLevels.push_back(std::move(levelData));
    }

    if (data->_pressureLevels.empty()) {
        return {};
    }

    const double latitude = root.value("latitude", NAN_VALUE);
    const double longitude = root.value("longitude", NAN_VALUE);
    const double elevationM = root.value("elevation", 0.0);
    if (std::isfinite(latitude) && std::isfinite(longitude)) {
        data->_dataLocation = SGGeod::fromDegM(longitude, latitude, elevationM);
    }

    return data;
}

bool OpenMeteoData::empty() const
{
    return _hourlyTimes.empty();
}

bool OpenMeteoData::timeBracket(std::time_t simTimeUtc,
                                std::size_t& lowerIndex,
                                std::size_t& upperIndex,
                                double& alpha) const
{
    lowerIndex = 0;
    upperIndex = 0;
    alpha = 0.0;

    if (_hourlyTimes.empty()) {
        return false;
    }

    if ((_hourlyTimes.size() == 1) || (simTimeUtc <= _hourlyTimes.front())) {
        return true;
    }

    if (simTimeUtc >= _hourlyTimes.back()) {
        lowerIndex = _hourlyTimes.size() - 1;
        upperIndex = lowerIndex;
        return true;
    }

    const auto upperIt = std::lower_bound(_hourlyTimes.begin(), _hourlyTimes.end(), simTimeUtc);
    if (upperIt == _hourlyTimes.end()) {
        lowerIndex = _hourlyTimes.size() - 1;
        upperIndex = lowerIndex;
        return true;
    }

    upperIndex = std::distance(_hourlyTimes.begin(), upperIt);
    if ((upperIndex == 0) || (*upperIt == simTimeUtc)) {
        lowerIndex = upperIndex;
        alpha = 0.0;
        return true;
    }

    lowerIndex = upperIndex - 1;
    const double denominator = std::difftime(_hourlyTimes[upperIndex], _hourlyTimes[lowerIndex]);
    if (std::fabs(denominator) <= SGLimitsd::min()) {
        alpha = 0.0;
    } else {
        alpha = SGMiscd::clip(std::difftime(simTimeUtc, _hourlyTimes[lowerIndex]) / denominator, 0.0, 1.0);
    }

    return true;
}

double OpenMeteoData::interpolateValue(const std::vector<double>& series,
                                       std::time_t simTimeUtc,
                                       double fallback) const
{
    if (series.empty()) {
        return fallback;
    }

    std::size_t lowerIndex = 0;
    std::size_t upperIndex = 0;
    double alpha = 0.0;
    if (!timeBracket(simTimeUtc, lowerIndex, upperIndex, alpha)) {
        return fallback;
    }

    const double low = (lowerIndex < series.size()) ? series[lowerIndex] : NAN_VALUE;
    const double high = (upperIndex < series.size()) ? series[upperIndex] : NAN_VALUE;
    const bool lowFinite = std::isfinite(low);
    const bool highFinite = std::isfinite(high);

    if (lowFinite && highFinite) {
        return SGMiscd::lerp(low, high, alpha);
    }

    if (lowFinite) {
        return low;
    }

    if (highFinite) {
        return high;
    }

    return fallback;
}

double OpenMeteoData::interpolateDirection(const std::vector<double>& series,
                                           std::time_t simTimeUtc,
                                           double fallback) const
{
    if (series.empty()) {
        return SGMiscd::normalizePeriodic(0.0, 360.0, fallback);
    }

    std::size_t lowerIndex = 0;
    std::size_t upperIndex = 0;
    double alpha = 0.0;
    if (!timeBracket(simTimeUtc, lowerIndex, upperIndex, alpha)) {
        return SGMiscd::normalizePeriodic(0.0, 360.0, fallback);
    }

    const double low = (lowerIndex < series.size()) ? series[lowerIndex] : NAN_VALUE;
    const double high = (upperIndex < series.size()) ? series[upperIndex] : NAN_VALUE;
    const bool lowFinite = std::isfinite(low);
    const bool highFinite = std::isfinite(high);

    if (lowFinite && highFinite) {
        const double normalizedLow = SGMiscd::normalizePeriodic(0.0, 360.0, low);
        const double delta = SGMiscd::normalizePeriodic(-180.0, 180.0, high - normalizedLow);
        return SGMiscd::normalizePeriodic(0.0, 360.0, normalizedLow + (delta * alpha));
    }

    if (lowFinite) {
        return SGMiscd::normalizePeriodic(0.0, 360.0, low);
    }

    if (highFinite) {
        return SGMiscd::normalizePeriodic(0.0, 360.0, high);
    }

    return SGMiscd::normalizePeriodic(0.0, 360.0, fallback);
}

double OpenMeteoData::maxAbsVerticalVelocity(std::time_t simTimeUtc) const
{
    double maximum = 0.0;
    for (const auto& level : _pressureLevels) {
        const double verticalVelocity = interpolateValue(level.verticalVelocityMps, simTimeUtc, 0.0);
        maximum = std::max(maximum, std::fabs(verticalVelocity));
    }

    return maximum;
}

OpenMeteoSurfaceData OpenMeteoData::surfaceData(std::time_t simTimeUtc) const
{
    OpenMeteoSurfaceData data;
    if (empty()) {
        return data;
    }

    data.seaLevelPressureHpa = interpolateValue(_pressureMslHpa, simTimeUtc, data.seaLevelPressureHpa);
    data.surfacePressureHpa = interpolateValue(_surfacePressureHpa, simTimeUtc, data.seaLevelPressureHpa);
    data.temperatureC = interpolateValue(_temperature2mC, simTimeUtc, data.temperatureC);
    data.dewPointC = interpolateValue(_dewPoint2mC, simTimeUtc, NAN_VALUE);
    data.relativeHumidityPercent = interpolateValue(_relativeHumidity2mPercent, simTimeUtc, NAN_VALUE);
    data.visibilityM = interpolateValue(_visibilityM, simTimeUtc, NAN_VALUE);
    data.precipitationMmPerHour = interpolateValue(_precipitationMm, simTimeUtc, 0.0);
    data.rainMmPerHour = interpolateValue(_rainMm, simTimeUtc, 0.0);
    data.showersMmPerHour = interpolateValue(_showersMm, simTimeUtc, 0.0);
    data.snowfallCmPerHour = interpolateValue(_snowfallCm, simTimeUtc, 0.0);
    data.weatherCode = interpolateValue(_weatherCode, simTimeUtc, 0.0);
    data.cloudCoverPercent = interpolateValue(_cloudCoverPercent, simTimeUtc, NAN_VALUE);
    data.cloudCoverLowPercent = interpolateValue(_cloudCoverLowPercent, simTimeUtc, NAN_VALUE);
    data.cloudCoverMidPercent = interpolateValue(_cloudCoverMidPercent, simTimeUtc, NAN_VALUE);
    data.cloudCoverHighPercent = interpolateValue(_cloudCoverHighPercent, simTimeUtc, NAN_VALUE);
    data.windDirectionDeg = interpolateDirection(_windDirection10mDeg, simTimeUtc, data.windDirectionDeg);
    data.windSpeedKt = interpolateValue(_windSpeed10mKt, simTimeUtc, data.windSpeedKt);
    data.windGustKt = interpolateValue(_windGust10mKt, simTimeUtc, data.windSpeedKt);
    data.capeJkg = interpolateValue(_capeJkg, simTimeUtc, 0.0);
    data.liftedIndex = interpolateValue(_liftedIndex, simTimeUtc, data.liftedIndex);
    data.convectiveInhibitionJkg = interpolateValue(_convectiveInhibitionJkg, simTimeUtc, 0.0);
    data.freezingLevelHeightM = interpolateValue(_freezingLevelHeightM, simTimeUtc, NAN_VALUE);
    data.maxVerticalVelocityMps = maxAbsVerticalVelocity(simTimeUtc);

    if (!std::isfinite(data.dewPointC)) {
        data.dewPointC = dewPointFromTemperatureAndHumidity(data.temperatureC, data.relativeHumidityPercent);
    }
    if (!std::isfinite(data.relativeHumidityPercent)) {
        data.relativeHumidityPercent = relativeHumidityFromTemperatureAndDewPoint(data.temperatureC, data.dewPointC);
    }
    if (!std::isfinite(data.relativeHumidityPercent)) {
        data.relativeHumidityPercent = 60.0;
    } else {
        data.relativeHumidityPercent = SGMiscd::clip(data.relativeHumidityPercent, 0.0, 100.0);
    }

    if (!std::isfinite(data.cloudCoverPercent)) {
        double totalCover = 0.0;
        int coverCount = 0;
        if (std::isfinite(data.cloudCoverLowPercent)) {
            totalCover += data.cloudCoverLowPercent;
            ++coverCount;
        }
        if (std::isfinite(data.cloudCoverMidPercent)) {
            totalCover += data.cloudCoverMidPercent;
            ++coverCount;
        }
        if (std::isfinite(data.cloudCoverHighPercent)) {
            totalCover += data.cloudCoverHighPercent;
            ++coverCount;
        }
        data.cloudCoverPercent = (coverCount > 0) ? (totalCover / coverCount) : 0.0;
    }

    if (!std::isfinite(data.cloudCoverPercent)) {
        data.cloudCoverPercent = 0.0;
    } else {
        data.cloudCoverPercent = SGMiscd::clip(data.cloudCoverPercent, 0.0, 100.0);
    }
    data.cloudCoverLowPercent = std::isfinite(data.cloudCoverLowPercent)
        ? SGMiscd::clip(data.cloudCoverLowPercent, 0.0, 100.0)
        : data.cloudCoverPercent;
    data.cloudCoverMidPercent = std::isfinite(data.cloudCoverMidPercent)
        ? SGMiscd::clip(data.cloudCoverMidPercent, 0.0, 100.0)
        : data.cloudCoverPercent;
    data.cloudCoverHighPercent = std::isfinite(data.cloudCoverHighPercent)
        ? SGMiscd::clip(data.cloudCoverHighPercent, 0.0, 100.0)
        : data.cloudCoverPercent;

    if (!std::isfinite(data.visibilityM) || (data.visibilityM <= 0.0)) {
        const double humidityPenalty = data.relativeHumidityPercent * 220.0;
        const double precipitationPenalty = data.precipitationMmPerHour * 4500.0;
        data.visibilityM = SGMiscd::clip(55000.0 - humidityPenalty - precipitationPenalty,
                                         150.0, 60000.0);
    } else {
        data.visibilityM = SGMiscd::clip(data.visibilityM, 150.0, 120000.0);
    }

    if (!std::isfinite(data.freezingLevelHeightM)) {
        data.freezingLevelHeightM = (data.temperatureC <= 0.0)
            ? 0.0
            : SGMiscd::clip((data.temperatureC / 6.5) * 1000.0, 0.0, 6000.0);
    }

    const double precipitationFallback = data.rainMmPerHour + data.showersMmPerHour + data.snowfallCmPerHour;
    if (!std::isfinite(data.precipitationMmPerHour)) {
        data.precipitationMmPerHour = std::max(0.0, precipitationFallback);
    }
    data.precipitationMmPerHour = std::max(0.0, data.precipitationMmPerHour);

    data.rainMmPerHour = std::max(0.0, data.rainMmPerHour);
    data.showersMmPerHour = std::max(0.0, data.showersMmPerHour);
    data.snowfallCmPerHour = std::max(0.0, data.snowfallCmPerHour);

    const int weatherCode = SGMiscd::roundToInt(data.weatherCode);
    data.thunderstormLikely = isThunderstormCode(weatherCode);
    data.convectiveSeverityNorm = convectiveSeverity(data.capeJkg,
                                                     data.liftedIndex,
                                                     data.convectiveInhibitionJkg,
                                                     data.maxVerticalVelocityMps,
                                                     data.thunderstormLikely);

    data.icingSeverityNorm = icingSeverity(data.temperatureC,
                                           data.relativeHumidityPercent,
                                           data.cloudCoverPercent,
                                           data.precipitationMmPerHour);

    data.rainNorm = SGMiscd::clip((data.rainMmPerHour + data.showersMmPerHour) / 10.0, 0.0, 1.0);
    data.snowNorm = SGMiscd::clip(data.snowfallCmPerHour / 2.5, 0.0, 1.0);
    if (data.temperatureC <= -1.0) {
        data.rainNorm *= 0.25;
    } else if (data.temperatureC >= 2.0) {
        data.snowNorm *= 0.25;
    }

    if (data.thunderstormLikely) {
        data.rainNorm = std::max(data.rainNorm, 0.4);
    }

    data.rainNorm = SGMiscd::clip(data.rainNorm, 0.0, 1.0);
    data.snowNorm = SGMiscd::clip(data.snowNorm, 0.0, 1.0);
    data.windGustKt = std::max(data.windGustKt, data.windSpeedKt);
    data.weatherCode = static_cast<double>(weatherCode);
    return data;
}

AtmosphericProfile OpenMeteoData::buildBoundaryProfile(std::time_t simTimeUtc) const
{
    AtmosphericProfile profile;
    if (empty()) {
        return profile;
    }

    const OpenMeteoSurfaceData surface = surfaceData(simTimeUtc);

    AtmosphericLayer surfaceLayer;
    surfaceLayer.altitudeFt = 0.0;
    surfaceLayer.pressureHpa = surface.seaLevelPressureHpa;
    surfaceLayer.temperatureC = surface.temperatureC;
    surfaceLayer.dewpointC = surface.dewPointC;
    surfaceLayer.relativeHumidityPercent = surface.relativeHumidityPercent;
    surfaceLayer.windDirectionDeg = surface.windDirectionDeg;
    surfaceLayer.windSpeedKt = surface.windSpeedKt;
    surfaceLayer.windGustKt = surface.windGustKt;
    surfaceLayer.visibilityM = surface.visibilityM;
    surfaceLayer.cloudCoveragePercent = surface.cloudCoverPercent;
    surfaceLayer.precipitationRateMmPerHour = surface.precipitationMmPerHour;
    surfaceLayer.verticalVelocityMps = 0.0;
    const double gustFactor = SGMiscd::clip((surface.windGustKt - surface.windSpeedKt) / 20.0, 0.0, 1.0);
    const double precipitationFactor = SGMiscd::clip(surface.precipitationMmPerHour / 8.0, 0.0, 1.0);
    const double cloudFactor = coverageScore(surface.cloudCoverPercent);
    surfaceLayer.turbulenceMagnitudeNorm = SGMiscd::clip((0.45 * surface.convectiveSeverityNorm)
                                                         + (0.25 * gustFactor)
                                                         + (0.20 * precipitationFactor)
                                                         + (0.10 * cloudFactor),
                                                         0.0, 1.0);
    surfaceLayer.turbulenceRateHz = SGMiscd::lerp(0.6, 5.0, surfaceLayer.turbulenceMagnitudeNorm);
    surfaceLayer.icingSeverityNorm = surface.icingSeverityNorm;
    profile.addLayer(surfaceLayer);

    AtmosphericLayer lowLevel = surfaceLayer;
    lowLevel.altitudeFt = 80.0 * SG_METER_TO_FEET;
    lowLevel.temperatureC = surface.temperatureC - 0.4;
    lowLevel.dewpointC = surface.dewPointC - 0.2;
    lowLevel.windSpeedKt = interpolateValue(_windSpeed80mKt, simTimeUtc, surface.windSpeedKt);
    lowLevel.windDirectionDeg = interpolateDirection(_windDirection80mDeg, simTimeUtc, surface.windDirectionDeg);
    lowLevel.windGustKt = std::max(lowLevel.windSpeedKt, surface.windGustKt);
    lowLevel.visibilityM = SGMiscd::clip(surface.visibilityM + 3000.0, 150.0, 80000.0);
    const double shearTerm = SGMiscd::clip(std::fabs(lowLevel.windSpeedKt - surface.windSpeedKt) / 18.0,
                                           0.0, 1.0);
    lowLevel.turbulenceMagnitudeNorm = SGMiscd::clip(surfaceLayer.turbulenceMagnitudeNorm + (0.2 * shearTerm),
                                                     0.0, 1.0);
    lowLevel.turbulenceRateHz = SGMiscd::lerp(0.6, 5.0, lowLevel.turbulenceMagnitudeNorm);
    profile.addLayer(lowLevel);

    profile.sortByAltitude();
    return profile;
}

AtmosphericProfile OpenMeteoData::buildAloftProfile(std::time_t simTimeUtc) const
{
    AtmosphericProfile profile;
    if (empty()) {
        return profile;
    }

    const OpenMeteoSurfaceData surface = surfaceData(simTimeUtc);

    std::vector<AtmosphericLayer> generatedLayers;
    generatedLayers.reserve(_pressureLevels.size());

    for (const auto& level : _pressureLevels) {
        AtmosphericLayer layer;
        layer.pressureHpa = static_cast<double>(level.pressureLevelHpa);
        layer.temperatureC = interpolateValue(level.temperatureC, simTimeUtc, surface.temperatureC);
        layer.dewpointC = interpolateValue(level.dewPointC, simTimeUtc, NAN_VALUE);
        layer.relativeHumidityPercent = interpolateValue(level.relativeHumidityPercent, simTimeUtc, NAN_VALUE);
        layer.windSpeedKt = interpolateValue(level.windSpeedKt, simTimeUtc, surface.windSpeedKt);
        layer.windDirectionDeg = interpolateDirection(level.windDirectionDeg, simTimeUtc, surface.windDirectionDeg);
        layer.windGustKt = std::max(layer.windSpeedKt, surface.windGustKt * 0.75);
        layer.cloudCoveragePercent = interpolateValue(level.cloudCoverPercent, simTimeUtc, surface.cloudCoverPercent);
        layer.verticalVelocityMps = interpolateValue(level.verticalVelocityMps, simTimeUtc, 0.0);

        const double geopotentialM = interpolateValue(level.geopotentialHeightM, simTimeUtc, NAN_VALUE);
        if (std::isfinite(geopotentialM) && (geopotentialM > 0.0)) {
            layer.altitudeFt = geopotentialM * SG_METER_TO_FEET;
        } else {
            layer.altitudeFt = fallbackAltitudeFt(level.pressureLevelHpa);
        }

        if (!std::isfinite(layer.dewpointC)) {
            layer.dewpointC = dewPointFromTemperatureAndHumidity(layer.temperatureC,
                                                                 layer.relativeHumidityPercent);
        }
        if (!std::isfinite(layer.relativeHumidityPercent)) {
            layer.relativeHumidityPercent = relativeHumidityFromTemperatureAndDewPoint(layer.temperatureC,
                                                                                       layer.dewpointC);
        }
        if (!std::isfinite(layer.relativeHumidityPercent)) {
            layer.relativeHumidityPercent = surface.relativeHumidityPercent;
        }
        layer.relativeHumidityPercent = SGMiscd::clip(layer.relativeHumidityPercent, 0.0, 100.0);

        if (!std::isfinite(layer.cloudCoveragePercent)) {
            layer.cloudCoveragePercent = surface.cloudCoverPercent;
        }
        layer.cloudCoveragePercent = SGMiscd::clip(layer.cloudCoveragePercent, 0.0, 100.0);

        const double altitudeFt = std::max(0.0, layer.altitudeFt);
        const double precipitationDecay = std::exp(-(altitudeFt / 14000.0));
        layer.precipitationRateMmPerHour = surface.precipitationMmPerHour
            * precipitationDecay
            * (0.35 + (0.65 * coverageScore(layer.cloudCoveragePercent)));

        double layerVisibility = SGMiscd::clip(surface.visibilityM + (altitudeFt * 2.5), 200.0, 80000.0);
        if ((layer.cloudCoveragePercent >= 70.0) && (layer.relativeHumidityPercent >= 90.0)) {
            layerVisibility = std::min(layerVisibility,
                                       1800.0 + ((100.0 - layer.cloudCoveragePercent) * 120.0));
        }
        if (layer.precipitationRateMmPerHour > 1.0) {
            layerVisibility = std::min(layerVisibility,
                                       6000.0 - (800.0 * std::min(4.0, layer.precipitationRateMmPerHour)));
        }
        layer.visibilityM = SGMiscd::clip(layerVisibility, 150.0, 80000.0);

        const double verticalMotionFactor = SGMiscd::clip(std::fabs(layer.verticalVelocityMps) / 2.0, 0.0, 1.0);
        const double cloudTurbulenceFactor = coverageScore(layer.cloudCoveragePercent)
            * humidityScore(layer.relativeHumidityPercent);
        const double convectiveAltitudeAttenuation = std::exp(-(altitudeFt / 30000.0));
        const double convectiveFactor = surface.convectiveSeverityNorm * convectiveAltitudeAttenuation;
        layer.turbulenceMagnitudeNorm = SGMiscd::clip((0.4 * verticalMotionFactor)
                                                      + (0.35 * cloudTurbulenceFactor)
                                                      + (0.25 * convectiveFactor),
                                                      0.0, 1.0);
        layer.turbulenceRateHz = SGMiscd::lerp(0.6, 5.0, layer.turbulenceMagnitudeNorm);
        layer.icingSeverityNorm = icingSeverity(layer.temperatureC,
                                                layer.relativeHumidityPercent,
                                                layer.cloudCoveragePercent,
                                                layer.precipitationRateMmPerHour);

        generatedLayers.push_back(layer);
    }

    std::sort(generatedLayers.begin(), generatedLayers.end(),
              [](const AtmosphericLayer& lhs, const AtmosphericLayer& rhs) {
                  return lhs.altitudeFt < rhs.altitudeFt;
              });

    for (std::size_t i = 1; i < generatedLayers.size(); ++i) {
        AtmosphericLayer& low = generatedLayers[i - 1];
        AtmosphericLayer& high = generatedLayers[i];

        const double deltaAltitudeKft = std::max((high.altitudeFt - low.altitudeFt) / 1000.0, 0.1);
        const double speedShear = std::fabs(high.windSpeedKt - low.windSpeedKt) / deltaAltitudeKft;
        const double headingShear = absDirectionDifference(low.windDirectionDeg, high.windDirectionDeg) / deltaAltitudeKft;
        const double shearFactor = SGMiscd::clip((speedShear / 35.0) + (headingShear / 150.0), 0.0, 1.0);

        high.turbulenceMagnitudeNorm = SGMiscd::clip((0.55 * high.turbulenceMagnitudeNorm)
                                                     + (0.45 * shearFactor),
                                                     0.0, 1.0);
        low.turbulenceMagnitudeNorm = std::max(low.turbulenceMagnitudeNorm,
                                               SGMiscd::clip(0.35 * shearFactor, 0.0, 1.0));
    }

    for (AtmosphericLayer& layer : generatedLayers) {
        layer.turbulenceRateHz = SGMiscd::lerp(0.6, 5.0, layer.turbulenceMagnitudeNorm);
        profile.addLayer(layer);
    }

    profile.sortByAltitude();
    return profile;
}

std::vector<OpenMeteoCloudLayer> OpenMeteoData::buildCloudLayers(std::time_t simTimeUtc,
                                                                 std::size_t maxLayers) const
{
    std::vector<OpenMeteoCloudLayer> output;
    if (empty() || (maxLayers == 0)) {
        return output;
    }

    const OpenMeteoSurfaceData surface = surfaceData(simTimeUtc);

    struct CloudSample {
        double altitudeFt = 0.0;
        double cloudCoverPercent = 0.0;
        double relativeHumidityPercent = 0.0;
        double verticalVelocityMps = 0.0;
    };

    std::vector<CloudSample> samples;
    samples.reserve(_pressureLevels.size());
    for (const auto& level : _pressureLevels) {
        const double geopotentialM = interpolateValue(level.geopotentialHeightM, simTimeUtc, NAN_VALUE);
        double altitudeFt = fallbackAltitudeFt(level.pressureLevelHpa);
        if (std::isfinite(geopotentialM) && (geopotentialM > 0.0)) {
            altitudeFt = geopotentialM * SG_METER_TO_FEET;
        }

        double cloudCoverPercent = interpolateValue(level.cloudCoverPercent, simTimeUtc, NAN_VALUE);
        double relativeHumidityPercent = interpolateValue(level.relativeHumidityPercent, simTimeUtc, NAN_VALUE);
        const double verticalVelocityMps = interpolateValue(level.verticalVelocityMps, simTimeUtc, 0.0);

        if (!std::isfinite(cloudCoverPercent) && !std::isfinite(relativeHumidityPercent)) {
            continue;
        }

        if (!std::isfinite(cloudCoverPercent)) {
            cloudCoverPercent = surface.cloudCoverPercent;
        }
        if (!std::isfinite(relativeHumidityPercent)) {
            relativeHumidityPercent = surface.relativeHumidityPercent;
        }

        if (!std::isfinite(cloudCoverPercent)) {
            cloudCoverPercent = 0.0;
        }
        if (!std::isfinite(relativeHumidityPercent)) {
            relativeHumidityPercent = 60.0;
        }

        CloudSample sample;
        sample.altitudeFt = altitudeFt;
        sample.cloudCoverPercent = SGMiscd::clip(cloudCoverPercent, 0.0, 100.0);
        sample.relativeHumidityPercent = SGMiscd::clip(relativeHumidityPercent, 0.0, 100.0);
        sample.verticalVelocityMps = verticalVelocityMps;
        samples.push_back(sample);
    }

    std::sort(samples.begin(), samples.end(),
              [](const CloudSample& lhs, const CloudSample& rhs) {
                  return lhs.altitudeFt < rhs.altitudeFt;
              });

    struct CandidateLayer {
        OpenMeteoCloudLayer layer;
        double score = 0.0;
    };

    std::vector<CandidateLayer> candidates;

    auto pushCandidate = [&candidates, &surface](double baseFt,
                                                  double topFt,
                                                  double averageCoverage,
                                                  double averageHumidity,
                                                  double averageVerticalVelocity) {
        OpenMeteoCloudLayer layer;
        layer.baseAltitudeFt = std::max(0.0, baseFt);
        layer.topAltitudeFt = std::max(layer.baseAltitudeFt + 300.0, topFt);
        layer.coveragePercent = SGMiscd::clip(averageCoverage, 0.0, 100.0);

        const double densityCoverage = coverageScore(layer.coveragePercent);
        const double densityHumidity = humidityScore(averageHumidity);
        layer.densityNorm = SGMiscd::clip((0.65 * densityCoverage) + (0.35 * densityHumidity), 0.0, 1.0);
        layer.turbulenceNorm = SGMiscd::clip((0.55 * SGMiscd::clip(std::fabs(averageVerticalVelocity) / 2.0, 0.0, 1.0))
                                             + (0.45 * surface.convectiveSeverityNorm),
                                             0.0, 1.0);
        layer.coverageType = OpenMeteoData::coverageTypeForPercent(layer.coveragePercent, layer.baseAltitudeFt);

        const double thicknessFt = layer.topAltitudeFt - layer.baseAltitudeFt;
        const double score = std::max(0.0, thicknessFt)
            * (0.5 + densityCoverage)
            * (0.5 + layer.densityNorm);
        candidates.push_back({layer, score});
    };

    if (!samples.empty()) {
        const double cloudCoverageThreshold = 15.0;
        const double cloudHumidityThreshold = 65.0;

        bool inLayer = false;
        double baseFt = 0.0;
        double topFt = 0.0;
        double coverageAccumulator = 0.0;
        double humidityAccumulator = 0.0;
        double verticalVelocityAccumulator = 0.0;
        int count = 0;

        for (std::size_t i = 0; i < samples.size(); ++i) {
            const CloudSample& sample = samples[i];
            const bool cloudy = (sample.cloudCoverPercent >= cloudCoverageThreshold)
                             && (sample.relativeHumidityPercent >= cloudHumidityThreshold);

            if (cloudy && !inLayer) {
                inLayer = true;
                baseFt = sample.altitudeFt;
                topFt = sample.altitudeFt;
                coverageAccumulator = 0.0;
                humidityAccumulator = 0.0;
                verticalVelocityAccumulator = 0.0;
                count = 0;
            }

            if (cloudy && inLayer) {
                topFt = sample.altitudeFt;
                coverageAccumulator += sample.cloudCoverPercent;
                humidityAccumulator += sample.relativeHumidityPercent;
                verticalVelocityAccumulator += sample.verticalVelocityMps;
                ++count;
            }

            const bool isLastSample = (i + 1 == samples.size());
            if (inLayer && (!cloudy || isLastSample)) {
                if (isLastSample && cloudy) {
                    topFt = sample.altitudeFt;
                }

                if (count > 0) {
                    pushCandidate(baseFt,
                                  std::max(baseFt + 600.0, topFt),
                                  coverageAccumulator / count,
                                  humidityAccumulator / count,
                                  verticalVelocityAccumulator / count);
                }

                inLayer = false;
            }
        }
    }

    if (candidates.empty() && (surface.cloudCoverPercent >= 10.0)) {
        const double baseAltitudeFt = std::max(300.0,
                                               122.0 * std::max(0.0, surface.temperatureC - surface.dewPointC)
                                               * SG_METER_TO_FEET);
        const double depthFt = SGMiscd::lerp(1200.0, 6500.0,
                                             SGMiscd::clip(surface.cloudCoverPercent / 100.0, 0.0, 1.0));
        pushCandidate(baseAltitudeFt,
                      baseAltitudeFt + depthFt,
                      surface.cloudCoverPercent,
                      surface.relativeHumidityPercent,
                      surface.maxVerticalVelocityMps);
    }

    if (surface.convectiveSeverityNorm >= 0.65) {
        const double baseAltitudeFt = std::max(1200.0,
                                               122.0 * std::max(0.0, surface.temperatureC - surface.dewPointC)
                                               * SG_METER_TO_FEET);
        const double topAltitudeFt = baseAltitudeFt + SGMiscd::lerp(9000.0, 22000.0, surface.convectiveSeverityNorm);
        pushCandidate(baseAltitudeFt,
                      topAltitudeFt,
                      std::max(70.0, surface.cloudCoverPercent),
                      std::max(80.0, surface.relativeHumidityPercent),
                      std::max(1.5, surface.maxVerticalVelocityMps));
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateLayer& lhs, const CandidateLayer& rhs) {
                  return lhs.score > rhs.score;
              });

    if (candidates.size() > maxLayers) {
        candidates.resize(maxLayers);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateLayer& lhs, const CandidateLayer& rhs) {
                  return lhs.layer.baseAltitudeFt < rhs.layer.baseAltitudeFt;
              });

    output.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        output.push_back(candidate.layer);
    }

    return output;
}

double OpenMeteoData::seaLevelPressureHpa(std::time_t simTimeUtc) const
{
    return interpolateValue(_pressureMslHpa, simTimeUtc, 1013.25);
}

double OpenMeteoData::fallbackAltitudeFt(int pressureLevelHpa)
{
    switch (pressureLevelHpa) {
    case 1000: return 360.0;
    case 975: return 1100.0;
    case 950: return 1800.0;
    case 925: return 2500.0;
    case 900: return 3250.0;
    case 850: return 5000.0;
    case 800: return 6500.0;
    case 750: return 8200.0;
    case 700: return 10000.0;
    case 650: return 11800.0;
    case 600: return 13800.0;
    case 550: return 15800.0;
    case 500: return 18000.0;
    case 450: return 20500.0;
    case 400: return 23500.0;
    case 350: return 27000.0;
    case 300: return 30000.0;
    case 250: return 34000.0;
    case 200: return 39000.0;
    default:  return 5000.0;
    }
}

double OpenMeteoData::dewPointFromTemperatureAndHumidity(double temperatureC,
                                                         double relativeHumidityPercent)
{
    if (!std::isfinite(temperatureC) || !std::isfinite(relativeHumidityPercent)) {
        return NAN_VALUE;
    }

    const double rh = SGMiscd::clip(relativeHumidityPercent, 1.0, 100.0);
    const double gamma = std::log(rh / 100.0) + ((17.625 * temperatureC) / (243.04 + temperatureC));
    return (243.04 * gamma) / (17.625 - gamma);
}

double OpenMeteoData::relativeHumidityFromTemperatureAndDewPoint(double temperatureC,
                                                                 double dewPointC)
{
    if (!std::isfinite(temperatureC) || !std::isfinite(dewPointC)) {
        return NAN_VALUE;
    }

    const double a = (7.5 * dewPointC) / (237.7 + dewPointC);
    const double b = (7.5 * temperatureC) / (237.7 + temperatureC);
    return 100.0 * std::pow(10.0, a - b);
}

double OpenMeteoData::convectiveSeverity(double capeJkg,
                                         double liftedIndex,
                                         double convectiveInhibitionJkg,
                                         double maxVerticalVelocityMps,
                                         bool thunderstormCode)
{
    const double capeTerm = SGMiscd::clip(capeJkg / 2000.0, 0.0, 1.0);
    const double liftedIndexTerm = SGMiscd::clip((-liftedIndex) / 8.0, 0.0, 1.0);
    const double inhibitionTerm = SGMiscd::clip(1.0 - (convectiveInhibitionJkg / 300.0), 0.0, 1.0);
    const double verticalVelocityTerm = SGMiscd::clip(std::fabs(maxVerticalVelocityMps) / 2.5, 0.0, 1.0);
    const double weatherCodeTerm = thunderstormCode ? 1.0 : 0.0;

    return SGMiscd::clip((0.35 * capeTerm)
                         + (0.30 * liftedIndexTerm)
                         + (0.15 * verticalVelocityTerm)
                         + (0.10 * inhibitionTerm)
                         + (0.10 * weatherCodeTerm),
                         0.0, 1.0);
}

double OpenMeteoData::icingSeverity(double temperatureC,
                                    double relativeHumidityPercent,
                                    double cloudCoverPercent,
                                    double precipitationRateMmPerHour)
{
    if ((temperatureC > 2.0) || (temperatureC < -25.0)) {
        return 0.0;
    }

    const double temperatureTerm = SGMiscd::clip(1.0 - (std::fabs(temperatureC + 7.0) / 18.0), 0.0, 1.0);
    const double humidityTerm = humidityScore(relativeHumidityPercent);
    const double cloudTerm = coverageScore(cloudCoverPercent);
    const double precipitationTerm = SGMiscd::clip(precipitationRateMmPerHour / 5.0, 0.0, 1.0);

    const double moisture = std::max(humidityTerm, cloudTerm);
    return SGMiscd::clip(temperatureTerm * ((0.65 * moisture) + (0.35 * precipitationTerm)), 0.0, 1.0);
}

bool OpenMeteoData::isThunderstormCode(int weatherCode)
{
    return (weatherCode == 95) || (weatherCode == 96) || (weatherCode == 99);
}

std::string OpenMeteoData::coverageTypeForPercent(double coveragePercent, double baseAltitudeFt)
{
    const double coverage = SGMiscd::clip(coveragePercent, 0.0, 100.0);
    if (coverage < 5.0) {
        return "clear";
    }

    if ((baseAltitudeFt > 20000.0) && (coverage < 60.0)) {
        return "cirrus";
    }

    if (coverage < 30.0) {
        return "few";
    }
    if (coverage < 55.0) {
        return "scattered";
    }
    if (coverage < 85.0) {
        return "broken";
    }

    return "overcast";
}

bool OpenMeteoData::parseTimestampUtc(const std::string& stamp, std::time_t& outUtc)
{
    if ((stamp.size() < 16) || (stamp[4] != '-') || (stamp[7] != '-')
        || (stamp[10] != 'T') || (stamp[13] != ':')) {
        return false;
    }

    try {
        const int year = std::stoi(stamp.substr(0, 4));
        const int month = std::stoi(stamp.substr(5, 2));
        const int day = std::stoi(stamp.substr(8, 2));
        const int hour = std::stoi(stamp.substr(11, 2));
        const int minute = std::stoi(stamp.substr(14, 2));

        outUtc = sgTimeGetGMT(year - 1900, month - 1, day, hour, minute, 0);
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }

    return true;
}

} // namespace Environment
