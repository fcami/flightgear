/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "weather_property_writer.hxx"

#include "atmospheric_profile.hxx"

#include <algorithm>

namespace {

double pressureHpaToInHg(double pressureHpa)
{
    return pressureHpa * 0.0295299830714;
}

} // namespace

namespace Environment {

WeatherPropertyWriter::WeatherPropertyWriter(SGPropertyNode_ptr configRoot) :
    _configRoot(configRoot)
{
}

void WeatherPropertyWriter::writeProfiles(const AtmosphericProfile& boundaryProfile,
                                          const AtmosphericProfile& aloftProfile,
                                          double seaLevelPressureHpa)
{
    if (!_configRoot.valid()) {
        return;
    }

    const double pressureInHg = pressureHpaToInHg(seaLevelPressureHpa);
    writeTable(_configRoot->getNode("boundary", true), boundaryProfile, pressureInHg, true);
    writeTable(_configRoot->getNode("aloft", true), aloftProfile, pressureInHg, false);
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

        entry->setDoubleValue("elevation-ft", layer.altitudeFt);
        entry->setDoubleValue("temperature-degc", layer.temperatureC);
        entry->setDoubleValue("wind-from-heading-deg", layer.windDirectionDeg);
        entry->setDoubleValue("wind-speed-kt", layer.windSpeedKt);
        entry->setDoubleValue("pressure-sea-level-inhg", pressureSeaLevelInHg);
    }
}

} // namespace Environment

