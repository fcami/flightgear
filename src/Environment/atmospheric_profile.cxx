/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "atmospheric_profile.hxx"

#include <algorithm>
#include <cmath>

namespace {

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

Environment::AtmosphericLayer interpolateLayer(const Environment::AtmosphericLayer& a,
                                               const Environment::AtmosphericLayer& b,
                                               double alpha,
                                               double altitudeFt)
{
    Environment::AtmosphericLayer out;
    out.altitudeFt = altitudeFt;
    out.pressureHpa = interpolateLinear(a.pressureHpa, b.pressureHpa, alpha);
    out.temperatureC = interpolateLinear(a.temperatureC, b.temperatureC, alpha);
    out.windDirectionDeg = interpolateHeading(a.windDirectionDeg, b.windDirectionDeg, alpha);
    out.windSpeedKt = interpolateLinear(a.windSpeedKt, b.windSpeedKt, alpha);
    return out;
}

} // namespace

namespace Environment {

void AtmosphericProfile::clear()
{
    _layers.clear();
}

void AtmosphericProfile::addLayer(const AtmosphericLayer& layer)
{
    _layers.push_back(layer);
}

void AtmosphericProfile::sortByAltitude()
{
    std::sort(_layers.begin(), _layers.end(),
              [](const AtmosphericLayer& lhs, const AtmosphericLayer& rhs) {
                  return lhs.altitudeFt < rhs.altitudeFt;
              });
}

bool AtmosphericProfile::empty() const
{
    return _layers.empty();
}

std::size_t AtmosphericProfile::size() const
{
    return _layers.size();
}

const AtmosphericLayer& AtmosphericProfile::at(std::size_t index) const
{
    return _layers.at(index);
}

AtmosphericLayer AtmosphericProfile::sample(double altitudeFt) const
{
    AtmosphericLayer fallback;
    fallback.altitudeFt = altitudeFt;

    if (_layers.empty()) {
        return fallback;
    }

    if ((_layers.size() == 1) || (altitudeFt <= _layers.front().altitudeFt)) {
        return _layers.front();
    }

    if (altitudeFt >= _layers.back().altitudeFt) {
        return _layers.back();
    }

    auto upper = std::lower_bound(_layers.begin(), _layers.end(), altitudeFt,
                                  [](const AtmosphericLayer& layer, double value) {
                                      return layer.altitudeFt < value;
                                  });
    if (upper == _layers.begin()) {
        return *upper;
    }

    const AtmosphericLayer& high = *upper;
    const AtmosphericLayer& low = *(upper - 1);
    const double denominator = high.altitudeFt - low.altitudeFt;
    if (std::fabs(denominator) < 1e-9) {
        return low;
    }

    const double alpha = (altitudeFt - low.altitudeFt) / denominator;
    return interpolateLayer(low, high, alpha, altitudeFt);
}

} // namespace Environment

