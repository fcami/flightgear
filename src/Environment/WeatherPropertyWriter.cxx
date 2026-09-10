/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "WeatherPropertyWriter.hxx"

#include "AtmosphericProfile.hxx"
#include "OpenMeteoData.hxx"

#include <algorithm>
#include <cmath>

#include <simgear/constants.h>
#include <simgear/math/SGMath.hxx>

namespace {

double finiteOrDefault(double value, double fallbackValue)
{
    return std::isfinite(value) ? value : fallbackValue;
}

} // namespace

namespace Environment {

WeatherPropertyWriter::WeatherPropertyWriter(SGPropertyNode_ptr configRoot,
                                             SGPropertyNode_ptr environmentRoot) :
    _configRoot(configRoot),
    _environmentRoot(environmentRoot)
{
}

void WeatherPropertyWriter::writeWeather(const AtmosphericProfile& boundaryProfile,
                                         const AtmosphericProfile& aloftProfile,
                                         double seaLevelPressureHpa,
                                         const OpenMeteoSurfaceData& surfaceData,
                                         const std::vector<OpenMeteoCloudLayer>& cloudLayers,
                                         bool lightningFlash,
                                         double icingSeverityNorm)
{
    if (!_configRoot.valid() || !_environmentRoot.valid()) {
        return;
    }

    const double pressureInHg = seaLevelPressureHpa * 100.0 * SG_PA_TO_INHG;
    writeTable(_configRoot->getNode("boundary", true), boundaryProfile, pressureInHg, true);
    writeTable(_configRoot->getNode("aloft", true), aloftProfile, pressureInHg, false);
    writeCloudLayers(cloudLayers, surfaceData);
    writeSurfaceWeather(surfaceData, lightningFlash, icingSeverityNorm);
}

void WeatherPropertyWriter::writeTable(SGPropertyNode_ptr tableNode,
                                       const AtmosphericProfile& profile,
                                       double pressureSeaLevelInHg,
                                       bool boundaryTable)
{
    if (!tableNode.valid() || profile.empty()) {
        return;
    }

    std::size_t entryCount = tableNode->getChildren("entry").size();
    entryCount = std::max(entryCount, profile.size());

    for (std::size_t i = 0; i < entryCount; ++i) {
        SGPropertyNode_ptr entry = tableNode->getChild("entry", static_cast<int>(i), true);

        AtmosphericLayer layer;
        if (i < profile.size()) {
            layer = profile.at(i);
        } else {
            const double sampleAltitude = entry->getDoubleValue("elevation-ft",
                                                                profile.at(profile.size() - 1).altitudeFt);
            layer = profile.sample(sampleAltitude);
        }

        if (boundaryTable) {
            layer.altitudeFt = std::max(0.0, layer.altitudeFt);
        }

        entry->setDoubleValue("elevation-ft", finiteOrDefault(layer.altitudeFt, 0.0));
        entry->setDoubleValue("temperature-degc", finiteOrDefault(layer.temperatureC, 15.0));
        entry->setDoubleValue("dewpoint-degc", finiteOrDefault(layer.dewpointC, 5.0));
        entry->setDoubleValue("visibility-m", finiteOrDefault(layer.visibilityM, 32000.0));
        entry->setDoubleValue("wind-from-heading-deg", finiteOrDefault(layer.windDirectionDeg, 0.0));
        entry->setDoubleValue("wind-speed-kt", finiteOrDefault(layer.windSpeedKt, 0.0));
        entry->setDoubleValue("pressure-sea-level-inhg", finiteOrDefault(pressureSeaLevelInHg, 29.92));
        entry->setDoubleValue("turbulence/magnitude-norm",
                              SGMiscd::clip(finiteOrDefault(layer.turbulenceMagnitudeNorm, 0.0), 0.0, 1.0));
        entry->setDoubleValue("turbulence/rate-hz",
                              SGMiscd::clip(finiteOrDefault(layer.turbulenceRateHz, 1.0), 0.1, 12.0));

        // Diagnostic properties used by Open-Meteo integration and downstream tooling.
        entry->setDoubleValue("wind-gust-kt", finiteOrDefault(layer.windGustKt, layer.windSpeedKt));
        entry->setDoubleValue("relative-humidity-pct",
                              SGMiscd::clip(finiteOrDefault(layer.relativeHumidityPercent, 0.0), 0.0, 100.0));
        entry->setDoubleValue("cloud-coverage-pct",
                              SGMiscd::clip(finiteOrDefault(layer.cloudCoveragePercent, 0.0), 0.0, 100.0));
        entry->setDoubleValue("precipitation-rate-mmph",
                              std::max(0.0, finiteOrDefault(layer.precipitationRateMmPerHour, 0.0)));
        entry->setDoubleValue("vertical-velocity-mps", finiteOrDefault(layer.verticalVelocityMps, 0.0));
        entry->setDoubleValue("icing/severity-norm",
                              SGMiscd::clip(finiteOrDefault(layer.icingSeverityNorm, 0.0), 0.0, 1.0));
    }
}

void WeatherPropertyWriter::writeCloudLayers(const std::vector<OpenMeteoCloudLayer>& cloudLayers,
                                             const OpenMeteoSurfaceData& surfaceData)
{
    SGPropertyNode_ptr cloudsNode = _environmentRoot->getNode("clouds", true);
    if (!cloudsNode.valid()) {
        return;
    }

    const std::size_t maxLayerCount = 5;
    for (std::size_t i = 0; i < maxLayerCount; ++i) {
        SGPropertyNode_ptr layerNode = cloudsNode->getChild("layer", static_cast<int>(i), true);

        OpenMeteoCloudLayer layer;
        if (i < cloudLayers.size()) {
            layer = cloudLayers[i];
        } else {
            layer.baseAltitudeFt = 2000.0 + (2000.0 * static_cast<double>(i));
            layer.topAltitudeFt = layer.baseAltitudeFt + 500.0;
            layer.coveragePercent = 0.0;
            layer.densityNorm = 0.0;
            layer.turbulenceNorm = 0.0;
            layer.coverageType = "clear";
        }

        layer.baseAltitudeFt = std::max(0.0, layer.baseAltitudeFt);
        layer.topAltitudeFt = std::max(layer.baseAltitudeFt + 200.0, layer.topAltitudeFt);
        const double thicknessFt = std::max(200.0, layer.topAltitudeFt - layer.baseAltitudeFt);
        const double densityNorm = SGMiscd::clip(layer.densityNorm, 0.0, 1.0);
        const double visibilityM = SGMiscd::clip(surfaceData.visibilityM * (1.0 - (0.65 * densityNorm)),
                                                 150.0, 80000.0);
        const double alpha = SGMiscd::clip(0.2 + (0.7 * densityNorm), 0.05, 0.95);

        layerNode->setDoubleValue("elevation-ft", layer.baseAltitudeFt);
        layerNode->setDoubleValue("thickness-ft", thicknessFt);
        layerNode->setDoubleValue("transition-ft", std::min(2000.0, thicknessFt * 0.4));
        layerNode->setStringValue("coverage", layer.coverageType.c_str());
        layerNode->setIntValue("coverage-type", coverageTypeAsInt(layer.coverageType));
        layerNode->setDoubleValue("visibility-m", visibilityM);
        layerNode->setDoubleValue("alpha", alpha);
        layerNode->setDoubleValue("openmeteo/coverage-pct", layer.coveragePercent);
        layerNode->setDoubleValue("openmeteo/density-norm", densityNorm);
        layerNode->setDoubleValue("openmeteo/turbulence-norm",
                                  SGMiscd::clip(layer.turbulenceNorm, 0.0, 1.0));
    }

    SGPropertyNode_ptr precipitationParams = _environmentRoot->getNode("params", true);
    if (precipitationParams.valid()) {
        const bool hasPrecipitation = (surfaceData.rainNorm > 0.01) || (surfaceData.snowNorm > 0.01);
        precipitationParams->setBoolValue("use-external-precipitation-level", hasPrecipitation);
        if (hasPrecipitation && !cloudLayers.empty()) {
            precipitationParams->setDoubleValue("external-precipitation-level-m",
                                                cloudLayers.front().topAltitudeFt * SG_FEET_TO_METER);
        }
    }
}

void WeatherPropertyWriter::writeSurfaceWeather(const OpenMeteoSurfaceData& surfaceData,
                                                bool lightningFlash,
                                                double icingSeverityNorm)
{
    _environmentRoot->setDoubleValue("rain-norm", SGMiscd::clip(surfaceData.rainNorm, 0.0, 1.0));
    _environmentRoot->setDoubleValue("snow-norm", SGMiscd::clip(surfaceData.snowNorm, 0.0, 1.0));
    _environmentRoot->setBoolValue("lightning/flash", lightningFlash);
    _environmentRoot->setDoubleValue("aircraft-effects/frost-level",
                                     SGMiscd::clip(icingSeverityNorm, 0.0, 1.0));

    SGPropertyNode_ptr openMeteoNode = _environmentRoot->getNode("open-meteo", true);
    openMeteoNode->setDoubleValue("surface/pressure-msl-hpa", surfaceData.seaLevelPressureHpa);
    openMeteoNode->setDoubleValue("surface/surface-pressure-hpa", surfaceData.surfacePressureHpa);
    openMeteoNode->setDoubleValue("surface/temperature-c", surfaceData.temperatureC);
    openMeteoNode->setDoubleValue("surface/dew-point-c", surfaceData.dewPointC);
    openMeteoNode->setDoubleValue("surface/relative-humidity-pct", surfaceData.relativeHumidityPercent);
    openMeteoNode->setDoubleValue("surface/visibility-m", surfaceData.visibilityM);
    openMeteoNode->setDoubleValue("surface/wind-speed-kt", surfaceData.windSpeedKt);
    openMeteoNode->setDoubleValue("surface/wind-gust-kt", surfaceData.windGustKt);
    openMeteoNode->setDoubleValue("surface/wind-direction-deg", surfaceData.windDirectionDeg);
    openMeteoNode->setDoubleValue("surface/precipitation-mmph", surfaceData.precipitationMmPerHour);
    openMeteoNode->setDoubleValue("surface/rain-mmph", surfaceData.rainMmPerHour);
    openMeteoNode->setDoubleValue("surface/showers-mmph", surfaceData.showersMmPerHour);
    openMeteoNode->setDoubleValue("surface/snowfall-cmph", surfaceData.snowfallCmPerHour);
    openMeteoNode->setDoubleValue("surface/weather-code", surfaceData.weatherCode);
    openMeteoNode->setDoubleValue("surface/cloud-cover-pct", surfaceData.cloudCoverPercent);
    openMeteoNode->setDoubleValue("surface/cloud-cover-low-pct", surfaceData.cloudCoverLowPercent);
    openMeteoNode->setDoubleValue("surface/cloud-cover-mid-pct", surfaceData.cloudCoverMidPercent);
    openMeteoNode->setDoubleValue("surface/cloud-cover-high-pct", surfaceData.cloudCoverHighPercent);
    openMeteoNode->setDoubleValue("surface/freezing-level-height-m", surfaceData.freezingLevelHeightM);
    openMeteoNode->setDoubleValue("convective/cape-jkg", surfaceData.capeJkg);
    openMeteoNode->setDoubleValue("convective/lifted-index", surfaceData.liftedIndex);
    openMeteoNode->setDoubleValue("convective/convective-inhibition-jkg", surfaceData.convectiveInhibitionJkg);
    openMeteoNode->setDoubleValue("convective/max-vertical-velocity-mps", surfaceData.maxVerticalVelocityMps);
    openMeteoNode->setDoubleValue("convective/severity-norm", surfaceData.convectiveSeverityNorm);
    openMeteoNode->setBoolValue("convective/thunderstorm-likely", surfaceData.thunderstormLikely);
    openMeteoNode->setDoubleValue("icing/surface-severity-norm", surfaceData.icingSeverityNorm);
    openMeteoNode->setDoubleValue("icing/aircraft-severity-norm", SGMiscd::clip(icingSeverityNorm, 0.0, 1.0));
}

int WeatherPropertyWriter::coverageTypeAsInt(const std::string& coverageType)
{
    // SGCloudLayer coverage enum values:
    // 0=overcast 1=broken 2=scattered 3=few 4=cirrus 5=clear
    if (coverageType == "overcast") {
        return 0;
    }
    if (coverageType == "broken") {
        return 1;
    }
    if (coverageType == "scattered") {
        return 2;
    }
    if (coverageType == "few") {
        return 3;
    }
    if (coverageType == "cirrus") {
        return 4;
    }
    return 5;
}

} // namespace Environment
