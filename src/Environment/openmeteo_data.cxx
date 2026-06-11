/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "openmeteo_data.hxx"

#include "atmospheric_profile.hxx"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include <simgear/constants.h>
#include <simgear/timing/sg_time.hxx>

namespace {

constexpr int PRESSURE_LEVELS[] = {
    1000, 975, 950, 925, 900, 850, 800, 750, 700, 650,
    600, 550, 500, 450, 400, 350, 300, 250, 200
};

double interpolateLinear(double a, double b, double alpha)
{
    return a + ((b - a) * alpha);
}

double normalizeHeading(double headingDeg)
{
    double normalized = std::fmod(headingDeg, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

double interpolateHeading(double aDeg, double bDeg, double alpha)
{
    const double a = normalizeHeading(aDeg);
    const double b = normalizeHeading(bDeg);
    const double delta = std::fmod((b - a) + 540.0, 360.0) - 180.0;
    return normalizeHeading(a + (delta * alpha));
}

bool readDoubleArray(const nlohmann::json& hourly,
                     const std::string& key,
                     std::size_t expectedSize,
                     std::vector<double>& outValues)
{
    const auto it = hourly.find(key);
    if ((it == hourly.end()) || !it->is_array()) {
        return false;
    }

    outValues.clear();
    outValues.reserve(it->size());
    for (const auto& v : *it) {
        if (!v.is_number()) {
            return false;
        }

        outValues.push_back(v.get<double>());
    }

    return outValues.size() == expectedSize;
}

double fallbackAltitudeFt(int pressureLevelHpa)
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
    if (!readDoubleArray(hourly, "temperature_2m", hourlyCount, data->_temperature2mC)) {
        return {};
    }
    if (!readDoubleArray(hourly, "pressure_msl", hourlyCount, data->_pressureMslHpa)) {
        return {};
    }
    if (!readDoubleArray(hourly, "wind_speed_10m", hourlyCount, data->_windSpeed10mKt)) {
        return {};
    }
    if (!readDoubleArray(hourly, "wind_direction_10m", hourlyCount, data->_windDirection10mDeg)) {
        return {};
    }

    if (!readDoubleArray(hourly, "wind_speed_80m", hourlyCount, data->_windSpeed80mKt)) {
        data->_windSpeed80mKt = data->_windSpeed10mKt;
    }
    if (!readDoubleArray(hourly, "wind_direction_80m", hourlyCount, data->_windDirection80mDeg)) {
        data->_windDirection80mDeg = data->_windDirection10mDeg;
    }

    for (int pressureLevelHpa : PRESSURE_LEVELS) {
        PressureLevelData levelData;
        levelData.pressureLevelHpa = pressureLevelHpa;

        const std::string levelSuffix = std::to_string(pressureLevelHpa) + "hPa";
        const bool hasTemperature = readDoubleArray(hourly, "temperature_" + levelSuffix,
                                                    hourlyCount, levelData.temperatureC);
        const bool hasWindSpeed = readDoubleArray(hourly, "wind_speed_" + levelSuffix,
                                                  hourlyCount, levelData.windSpeedKt);
        const bool hasWindDirection = readDoubleArray(hourly, "wind_direction_" + levelSuffix,
                                                      hourlyCount, levelData.windDirectionDeg);
        const bool hasGeopotential = readDoubleArray(hourly, "geopotential_height_" + levelSuffix,
                                                     hourlyCount, levelData.geopotentialHeightM);

        if (hasTemperature && hasWindSpeed && hasWindDirection && hasGeopotential) {
            data->_pressureLevels.push_back(std::move(levelData));
        }
    }

    if (data->_pressureLevels.empty()) {
        return {};
    }

    const double latitude = root.value("latitude", std::numeric_limits<double>::quiet_NaN());
    const double longitude = root.value("longitude", std::numeric_limits<double>::quiet_NaN());
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
    if (std::fabs(denominator) < 1e-9) {
        alpha = 0.0;
    } else {
        alpha = std::difftime(simTimeUtc, _hourlyTimes[lowerIndex]) / denominator;
    }

    if (alpha < 0.0) {
        alpha = 0.0;
    } else if (alpha > 1.0) {
        alpha = 1.0;
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

    if (lowerIndex >= series.size()) {
        return series.back();
    }
    if (upperIndex >= series.size()) {
        return series[lowerIndex];
    }

    return interpolateLinear(series[lowerIndex], series[upperIndex], alpha);
}

double OpenMeteoData::interpolateDirection(const std::vector<double>& series,
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

    if (lowerIndex >= series.size()) {
        return normalizeHeading(series.back());
    }
    if (upperIndex >= series.size()) {
        return normalizeHeading(series[lowerIndex]);
    }

    return interpolateHeading(series[lowerIndex], series[upperIndex], alpha);
}

AtmosphericProfile OpenMeteoData::buildBoundaryProfile(std::time_t simTimeUtc) const
{
    AtmosphericProfile profile;
    if (empty()) {
        return profile;
    }

    AtmosphericLayer surface;
    surface.altitudeFt = 0.0;
    surface.pressureHpa = seaLevelPressureHpa(simTimeUtc);
    surface.temperatureC = interpolateValue(_temperature2mC, simTimeUtc);
    surface.windSpeedKt = interpolateValue(_windSpeed10mKt, simTimeUtc);
    surface.windDirectionDeg = interpolateDirection(_windDirection10mDeg, simTimeUtc);
    profile.addLayer(surface);

    AtmosphericLayer lowLevel = surface;
    lowLevel.altitudeFt = 80.0 * SG_METER_TO_FEET;
    lowLevel.windSpeedKt = interpolateValue(_windSpeed80mKt, simTimeUtc, surface.windSpeedKt);
    lowLevel.windDirectionDeg = interpolateDirection(_windDirection80mDeg, simTimeUtc,
                                                     surface.windDirectionDeg);
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

    for (const auto& level : _pressureLevels) {
        AtmosphericLayer layer;
        layer.pressureHpa = static_cast<double>(level.pressureLevelHpa);
        layer.temperatureC = interpolateValue(level.temperatureC, simTimeUtc);
        layer.windSpeedKt = interpolateValue(level.windSpeedKt, simTimeUtc);
        layer.windDirectionDeg = interpolateDirection(level.windDirectionDeg, simTimeUtc);

        const double geopotentialM = interpolateValue(level.geopotentialHeightM, simTimeUtc,
                                                      std::numeric_limits<double>::quiet_NaN());
        if (std::isfinite(geopotentialM) && (geopotentialM > 0.0)) {
            layer.altitudeFt = geopotentialM * SG_METER_TO_FEET;
        } else {
            layer.altitudeFt = fallbackAltitudeFt(level.pressureLevelHpa);
        }

        profile.addLayer(layer);
    }

    profile.sortByAltitude();
    return profile;
}

double OpenMeteoData::seaLevelPressureHpa(std::time_t simTimeUtc) const
{
    return interpolateValue(_pressureMslHpa, simTimeUtc, 1013.25);
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
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

} // namespace Environment

