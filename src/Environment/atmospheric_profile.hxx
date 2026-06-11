// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <vector>

namespace Environment {

struct AtmosphericLayer {
    double altitudeFt = 0.0;
    double pressureHpa = 0.0;
    double temperatureC = 0.0;
    double windDirectionDeg = 0.0;
    double windSpeedKt = 0.0;
};

class AtmosphericProfile
{
public:
    void clear();
    void addLayer(const AtmosphericLayer& layer);
    void sortByAltitude();

    bool empty() const;
    std::size_t size() const;
    const AtmosphericLayer& at(std::size_t index) const;

    AtmosphericLayer sample(double altitudeFt) const;

private:
    std::vector<AtmosphericLayer> _layers;
};

} // namespace Environment

