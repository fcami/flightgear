/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include <simgear/props/props.hxx>

namespace Environment {

class AtmosphericProfile;
struct OpenMeteoCloudLayer;
struct OpenMeteoSurfaceData;

class WeatherPropertyWriter
{
public:
    WeatherPropertyWriter(SGPropertyNode_ptr configRoot, SGPropertyNode_ptr environmentRoot);

    void writeWeather(const AtmosphericProfile& boundaryProfile,
                      const AtmosphericProfile& aloftProfile,
                      double seaLevelPressureHpa,
                      const OpenMeteoSurfaceData& surfaceData,
                      const std::vector<OpenMeteoCloudLayer>& cloudLayers,
                      bool lightningFlash,
                      double icingSeverityNorm);

private:
    void writeTable(SGPropertyNode_ptr tableNode,
                    const AtmosphericProfile& profile,
                    double pressureSeaLevelInHg,
                    bool boundaryTable);
    void writeCloudLayers(const std::vector<OpenMeteoCloudLayer>& cloudLayers,
                          const OpenMeteoSurfaceData& surfaceData);
    void writeSurfaceWeather(const OpenMeteoSurfaceData& surfaceData,
                             bool lightningFlash,
                             double icingSeverityNorm);
    static int coverageTypeAsInt(const std::string& coverageType);

    SGPropertyNode_ptr _configRoot;
    SGPropertyNode_ptr _environmentRoot;
};

} // namespace Environment
