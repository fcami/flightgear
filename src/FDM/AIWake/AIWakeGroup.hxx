// AIWakeGroup.hxx -- Group of AI wake meshes for the computation of the induced
// wake.
//
// Written by Bertrand Coconnier, started April 2017.
//
// SPDX-FileCopyrightText: (C) 2017  Bertrand Coconnier  - bcoconni@users.sf.net
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>

#include <simgear/math/SGGeod.hxx>
#include <simgear/math/SGQuat.hxx>
#include <simgear/props/propsfwd.hxx>

#include "FDM/AIWake/WakeMesh.hxx"

namespace FGTestApi {
namespace PrivateAccessor {
namespace FDM {
class Accessor;
}
} // namespace PrivateAccessor
} // namespace FGTestApi
class FGAIAircraft;

class AIWakeGroup
{
    friend class FGTestApi::PrivateAccessor::FDM::Accessor;

    struct AIWakeData {
        explicit AIWakeData(WakeMesh* m = nullptr) : mesh(m) {}

        SGVec3d position{SGVec3d::zeros()};
        SGQuatd Te2b{SGQuatd::unit()};
        bool visited{false};
        WakeMesh_ptr mesh;
    };

    std::map<int, AIWakeData> _aiWakeData;
    SGPropertyNode_ptr _density_slugft;

public:
    AIWakeGroup(void);
    void AddAI(FGAIAircraft* ai);
    SGVec3d getInducedVelocityAt(const SGVec3d& pt) const;
    // Garbage collection
    void gc(void);
};
