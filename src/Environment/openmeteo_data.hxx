// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include <simgear/math/SGMath.hxx>

namespace Environment {

class AtmosphericProfile;

class OpenMeteoData
{
public:
    static std::unique_ptr<OpenMeteoData> parse(const std::string& payload);

    bool empty() const;

    AtmosphericProfile buildBoundaryProfile(std::time_t simTimeUtc) const;
    AtmosphericProfile buildAloftProfile(std::time_t simTimeUtc) const;

    double seaLevelPressureHpa(std::time_t simTimeUtc) const;
    const SGGeod& dataLocation() const { return _dataLocation; }

private:
    struct PressureLevelData {
        int pressureLevelHpa = 0;
        std::vector<double> temperatureC;
        std::vector<double> windSpeedKt;
        std::vector<double> windDirectionDeg;
        std::vector<double> geopotentialHeightM;
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

    static bool parseTimestampUtc(const std::string& stamp, std::time_t& outUtc);

    std::vector<std::time_t> _hourlyTimes;
    std::vector<double> _temperature2mC;
    std::vector<double> _pressureMslHpa;
    std::vector<double> _windSpeed10mKt;
    std::vector<double> _windDirection10mDeg;
    std::vector<double> _windSpeed80mKt;
    std::vector<double> _windDirection80mDeg;
    std::vector<PressureLevelData> _pressureLevels;

    SGGeod _dataLocation;
};

} // namespace Environment

