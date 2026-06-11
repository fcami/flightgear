// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/props/props.hxx>

namespace Environment {

class AtmosphericProfile;

class WeatherPropertyWriter
{
public:
    explicit WeatherPropertyWriter(SGPropertyNode_ptr configRoot);

    void writeProfiles(const AtmosphericProfile& boundaryProfile,
                       const AtmosphericProfile& aloftProfile,
                       double seaLevelPressureHpa);

private:
    void writeTable(SGPropertyNode_ptr tableNode,
                    const AtmosphericProfile& profile,
                    double pressureSeaLevelInHg,
                    bool boundaryTable);

    SGPropertyNode_ptr _configRoot;
};

} // namespace Environment

