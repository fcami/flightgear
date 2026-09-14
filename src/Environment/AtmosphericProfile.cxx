/*
 * SPDX-FileCopyrightText: 2026 The FlightGear Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "AtmosphericProfile.hxx"

#include <algorithm>
#include <cmath>

#include <simgear/math/SGMath.hxx>

namespace {

double interpolateScalar(double a, double b, double alpha, double fallback = 0.0)
{
    const bool aFinite = std::isfinite(a);
    const bool bFinite = std::isfinite(b);

    if (aFinite && bFinite) {
        return SGMiscd::lerp(a, b, alpha);
    }

    if (aFinite) {
        return a;
    }

    if (bFinite) {
        return b;
    }

    return fallback;
}

double normalizeHeading(double headingDeg)
{
    return SGMiscd::normalizePeriodic(0.0, 360.0, headingDeg);
}

double interpolateHeading(double aDeg, double bDeg, double alpha)
{
    const bool aFinite = std::isfinite(aDeg);
    const bool bFinite = std::isfinite(bDeg);

    if (!aFinite && !bFinite) {
        return 0.0;
    }

    if (!aFinite) {
        return normalizeHeading(bDeg);
    }

    if (!bFinite) {
        return normalizeHeading(aDeg);
    }

    const double normalizedA = normalizeHeading(aDeg);
    const double delta = SGMiscd::normalizePeriodic(-180.0, 180.0, bDeg - normalizedA);
    return normalizeHeading(normalizedA + (delta * alpha));
}

Environment::AtmosphericLayer interpolateLayer(const Environment::AtmosphericLayer& low,
                                               const Environment::AtmosphericLayer& high,
                                               double alpha,
                                               double altitudeFt)
{
    Environment::AtmosphericLayer out;
    out.altitudeFt = altitudeFt;
    out.pressureHpa = interpolateScalar(low.pressureHpa, high.pressureHpa, alpha);
    out.temperatureC = interpolateScalar(low.temperatureC, high.temperatureC, alpha);
    out.dewpointC = interpolateScalar(low.dewpointC, high.dewpointC, alpha);
    out.relativeHumidityPercent = interpolateScalar(low.relativeHumidityPercent,
                                                    high.relativeHumidityPercent,
                                                    alpha);
    out.windDirectionDeg = interpolateHeading(low.windDirectionDeg, high.windDirectionDeg, alpha);
    out.windSpeedKt = interpolateScalar(low.windSpeedKt, high.windSpeedKt, alpha);
    out.windGustKt = interpolateScalar(low.windGustKt, high.windGustKt, alpha);
    out.visibilityM = interpolateScalar(low.visibilityM, high.visibilityM, alpha, 32000.0);
    out.cloudCoveragePercent = interpolateScalar(low.cloudCoveragePercent, high.cloudCoveragePercent, alpha);
    out.precipitationRateMmPerHour = interpolateScalar(low.precipitationRateMmPerHour,
                                                       high.precipitationRateMmPerHour,
                                                       alpha);
    out.verticalVelocityMps = interpolateScalar(low.verticalVelocityMps, high.verticalVelocityMps, alpha);
    out.turbulenceMagnitudeNorm = interpolateScalar(low.turbulenceMagnitudeNorm,
                                                    high.turbulenceMagnitudeNorm,
                                                    alpha);
    out.turbulenceRateHz = interpolateScalar(low.turbulenceRateHz, high.turbulenceRateHz, alpha, 1.0);
    out.icingSeverityNorm = interpolateScalar(low.icingSeverityNorm, high.icingSeverityNorm, alpha);
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

AtmosphericLayer& AtmosphericProfile::at(std::size_t index)
{
    return _layers.at(index);
}

const AtmosphericLayer& AtmosphericProfile::at(std::size_t index) const
{
    return _layers.at(index);
}

AtmosphericProfile::iterator AtmosphericProfile::begin()
{
    return _layers.begin();
}

AtmosphericProfile::iterator AtmosphericProfile::end()
{
    return _layers.end();
}

AtmosphericProfile::const_iterator AtmosphericProfile::begin() const
{
    return _layers.begin();
}

AtmosphericProfile::const_iterator AtmosphericProfile::end() const
{
    return _layers.end();
}

AtmosphericProfile::const_iterator AtmosphericProfile::cbegin() const
{
    return _layers.cbegin();
}

AtmosphericProfile::const_iterator AtmosphericProfile::cend() const
{
    return _layers.cend();
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
    if (std::fabs(denominator) <= SGLimitsd::min()) {
        return low;
    }

    const double alpha = SGMiscd::clip((altitudeFt - low.altitudeFt) / denominator, 0.0, 1.0);
    return interpolateLayer(low, high, alpha, altitudeFt);
}

} // namespace Environment

