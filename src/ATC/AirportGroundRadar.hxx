// SPDX-FileCopyrightText: 2023 Keith Paterson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <simgear/math/SGGeod.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

#include "AIModel/AIBase.hxx"
#include "ATC/QuadTree.hxx"
#include "ATC/trafficcontrol.hxx"
#include "Airports/airports_fwd.hxx"


using quadtree::QuadTree;
using FGTrafficRef = SGSharedPtr<FGTrafficRecord>;

/**
 * Class representing a kind of ground radar. It is used to control traffic by FGGroundController
 * and prevent collisions. It supporst all FGAIBase objects.
*/
class AirportGroundRadar : public SGReferenced
{
public:
    // for index
    /**Function implementing calculation of dimension for Quadtree*/
    static SGRect<double> getBox(FGTrafficRef aiObject)
    {
        /*
		if ((*aiObject).getCallsign() == "BA780") {
		  SG_LOG(SG_ATC, SG_DEV_WARN, "getBox " << (*aiObject).getCallsign() << "(" << (*aiObject).getId() << ") " << aiObject << " " << (*aiObject).getPos().getLatitudeDeg() << " " << (*aiObject).getPos().getLongitudeDeg());
		}
		  */
        return SGRect<double>((*aiObject).getPos().getLatitudeDeg(),
                              (*aiObject).getPos().getLongitudeDeg());
    };
    /**Function implementing equals for Quadtree*/
    static bool equal(FGTrafficRef o, FGTrafficRef o2)
    {
        //		if ((*o).getCallsign() == "BA780") {
        //SG_LOG(SG_ATC, SG_DEBUG, (*o).getCallsign() << "(" << (*o).getId() << ")" << o << "/" << (*o2).getCallsign() << "(" << (*o2).getId() << ")" << o2);
        //		}
        return (*o).getId() == (*o2).getId();
    };

private:
    const double QUERY_BOX_SIZE = 0.1;
    const int SEPARATION = 4;
    QuadTree<FGTrafficRecord, decltype(&getBox), decltype(&equal)> index;
    SGGeod min;
    FGAirportRef airport;
    int getSize(FGTrafficRef aiObject);
    bool blocking(FGTrafficRef aiObject, FGTrafficRef other);

public:
    AirportGroundRadar(SGGeod min, SGGeod max);
    AirportGroundRadar(FGAirportRef airport);
    ~AirportGroundRadar();
    bool add(FGTrafficRef aiObject);
    bool move(const SGRectd& newPos, FGTrafficRef aiObject);
    bool remove(FGTrafficRef aiObject);
    size_t size() const;
    /**Returns if this AI object is blocked by any other "known" aka visible to the Radar.*/
    bool isBlocked(FGTrafficRef aiObject);
    bool isBlockedForPushback(FGTrafficRef aiObject);
    /**Returns which AI object is blocking this traffic.*/
    const FGTrafficRef getBlockedBy(FGTrafficRef aiObject);
};
