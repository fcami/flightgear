/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cstddef>
#include <vector>

namespace Environment {

struct AtmosphericLayer {
    double altitudeFt = 0.0;
    double pressureHpa = 0.0;
    double temperatureC = 0.0;
    double dewpointC = 0.0;
    double relativeHumidityPercent = 0.0;
    double windDirectionDeg = 0.0;
    double windSpeedKt = 0.0;
    double windGustKt = 0.0;
    double visibilityM = 32000.0;
    double cloudCoveragePercent = 0.0;
    double precipitationRateMmPerHour = 0.0;
    double verticalVelocityMps = 0.0;
    double turbulenceMagnitudeNorm = 0.0;
    double turbulenceRateHz = 1.0;
    double icingSeverityNorm = 0.0;
};

class AtmosphericProfile
{
public:
    using LayerContainer = std::vector<AtmosphericLayer>;
    using iterator = LayerContainer::iterator;
    using const_iterator = LayerContainer::const_iterator;

    void clear();
    void addLayer(const AtmosphericLayer& layer);
    void sortByAltitude();

    bool empty() const;
    std::size_t size() const;

    AtmosphericLayer& at(std::size_t index);
    const AtmosphericLayer& at(std::size_t index) const;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    AtmosphericLayer sample(double altitudeFt) const;

private:
    LayerContainer _layers;
};

} // namespace Environment

