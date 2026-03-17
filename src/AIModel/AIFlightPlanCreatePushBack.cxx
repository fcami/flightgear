/****************************************************************************
* AIFlightPlanCreatePushBack.cxx
* SPDX-FileCopyrightText: Written by Durk Talsma,  started August 1, 2007
* SPDX-License-Identifier: GPL-2.0-or-later
**************************************************************************/

#include <config.h>

#include <cstdio>
#include <cstdlib>

#include <simgear/math/sg_geodesy.hxx>

#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/groundnetwork.hxx>
#include <Airports/runways.hxx>

#include <Environment/environment.hxx>
#include <Environment/environment_mgr.hxx>

#include <Traffic/Schedule.hxx>

#include "AIAircraft.hxx"
#include "AIFlightPlan.hxx"
#include "performancedata.hxx"

using std::string;

// TODO: Use James Turner's createOnGround functions.
bool FGAIFlightPlan::createPushBack(FGAIAircraft* ac,
                                    bool firstFlight,
                                    FGAirport* dep,
                                    double radius,
                                    const string& fltType,
                                    const string& aircraftType,
                                    const string& airline)
{
    double vTaxi = ac->getPerformance()->vTaxi();
    double vTaxiBackward = vTaxi * (-2.0 / 3.0);
    double vTaxiReduced = vTaxi * (2.0 / 3.0);

    // Active runway can be conditionally set by ATC, so at the start of a new flight, this
    // must be reset.
    activeRunway.clear();
    FGRunway* rwy = ensureActiveRunway(ac, dep, fltType);
    if (!rwy) {
        SG_LOG(SG_AI, SG_DEV_WARN, "Could not find active runway for " << ac->getTrafficRef()->getCallSign() << " at " << dep->getId());
        return false;
    }

    if (!(dep->getDynamics()->getGroundController()->exists())) {
        //cerr << "Push Back fallback" << endl;
        SG_LOG(SG_AI, SG_DEV_WARN, "No groundcontroller createPushBackFallBack at " << dep->getId());
        createPushBackFallBack(ac, firstFlight, dep,
                               radius, fltType, aircraftType, airline);
        return true;
    }

    if (firstFlight || !dep->getDynamics()->hasParking(gate.parking())) {
        // establish the parking position / gate
        // if the airport has no parking positions defined, don't log
        // the warning below.
        if (!dep->getDynamics()->hasParkings()) {
            return false;
        }
        gate = dep->getDynamics()->getAvailableParking(radius, fltType,
                                                       aircraftType, airline);
        if (!gate.isValid()) {
            SG_LOG(SG_AI, SG_DEV_WARN, "Could not find parking for a " << aircraftType << " of flight type " << fltType << " of airline     " << airline << " at airport     " << dep->getId());
            return false;
        }
    }


    if (!gate.isValid()) {
        SG_LOG(SG_AI, SG_DEV_WARN, "Gate " << gate.parking()->ident() << " not valid createPushBackFallBack at " << dep->getId());
        createPushBackFallBack(ac, firstFlight, dep,
                               radius, fltType, aircraftType, airline);
        return true;
    }

    FGGroundNetwork* groundNet = dep->groundNetwork();
    FGParking* parking = gate.parking();
    if (!parking) {
        SG_LOG(SG_AI, SG_DEV_WARN, "Parking pointer is null in createPushBack for " << dep->getId());
        return false;
    }
    if (parking && parking->getPushBackPoint() != nullptr) {
        // Classic pushback with defined pushback point
        FGTaxiRoute route = groundNet->findShortestRoute(parking, parking->getPushBackPoint(), false);
        SG_LOG(SG_AI, SG_BULK, "Creating Pushback from " << parking->ident() << " to " << parking->getPushBackPoint()->getIndex());

        int size = route.size();
        if (size < 2) {
            SG_LOG(SG_AI, SG_DEV_WARN, "Push back route from gate " << parking->ident() << " has only " << size << " nodes.\n"
                                                                    << "Using  " << parking->getPushBackPoint());
        }

        route.first();
        FGTaxiNodeRef node;
        int rte;

        if (!waypoints.empty()) {
            // This will be a parking from a previous leg which still contains the forward speed
            waypoints.back()->setSpeed(vTaxiBackward);
        }

        while (route.next(node, &rte)) {
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "pushback-%03d", (short)node->getIndex());
            FGAIWaypoint* wpt = createOnGround(ac, string(buffer), node->geod(), dep->getElevation(), vTaxiBackward);
            wpt->setRouteIndex(rte);
            pushBackWaypoint(wpt);
        }
        // some special considerations for the last point:
        // This will trigger the release of parking
        waypoints.back()->setName(string("PushBackPoint"));
        waypoints.back()->setSpeed(vTaxi);
        ac->setTaxiClearanceRequest(true);
    } else { // In case of a push forward departure... or missing pushback route
        ac->setTaxiClearanceRequest(false);
        double az2 = 0.0;

        FGGroundNetwork* groundnet = dep->groundNetwork();
        FGTaxiSegment* pushForwardSegment = groundnet->findSegmentByHeading(parking, parking->getHeading());

        if (!pushForwardSegment) {
            // there aren't any routes for this parking, so create a simple segment straight ahead for 2 meters based on the parking heading
            if (parking->getType() == "gate") {
                SG_LOG(SG_AI, SG_DEBUG, "Gate " << parking->ident() << " is a pushback gate.");

                auto intersection = groundnet->findIntersectionSegment(parking->geod(), parking->getReverseHeading());
                if (!intersection) {
                    SG_LOG(SG_AI, SG_DEV_WARN, "No pushforward intersection found for pushback gate " << dep->getId() << "/" << parking->ident());
                    return false;
                }

                FGTaxiRoute routeFromStart = findBestTaxiRouteToRunway(ac, dep, rwy, groundnet, intersection->getStart());
                FGTaxiRoute routeFromEnd = findBestTaxiRouteToRunway(ac, dep, rwy, groundnet, intersection->getEnd());

                if (routeFromStart.empty() && routeFromEnd.empty()) {
                    SG_LOG(SG_AI, SG_DEV_WARN, "No route from pushforward intersection to runway.");
                }
                const double curveRadius = 20.0;
                if (routeFromStart.getDistance() > routeFromEnd.getDistance()) {
                    const double segmentHeading = SGGeodesy::courseDeg(intersection->getStart()->geod(), intersection->getEnd()->geod());
                    const double enclosedAngle = SGMiscd::normalizePeriodic(0.0, 360.0, segmentHeading - parking->getHeading());
                    const double distance = (curveRadius / std::sin(enclosedAngle / 2.0));
                    auto circleCenter = SGGeodesy::direct(intersection->getIntersection(), parking->getHeading() - (enclosedAngle / 2.0), distance);
                    createArc(ac, circleCenter, parking->getReverseHeading() - 90, parking->getReverseHeading() - 90 + enclosedAngle, 10, curveRadius, dep->getElevation(), vTaxiReduced, "circlePushback%d");
                } else {
                    const double segmentHeading = SGGeodesy::courseDeg(intersection->getEnd()->geod(), intersection->getStart()->geod());
                    const double enclosedAngle = SGMiscd::normalizePeriodic(0.0, 360.0, segmentHeading - parking->getHeading());
                    const double distance = (curveRadius / std::sin(enclosedAngle / 2.0));
                    auto circleCenter = SGGeodesy::direct(intersection->getIntersection(), parking->getHeading() + (enclosedAngle / 2.0), distance);
                    createArc(ac, circleCenter, parking->getReverseHeading() - 90, parking->getReverseHeading() - 90 - enclosedAngle, -10, curveRadius, dep->getElevation(), vTaxiReduced, "circlePushback%d");
                }
                return true;
            } else {
                SG_LOG(SG_AI, SG_DEV_WARN, "Gate " << parking->ident() << " is NOT a pushback gate.");
                return false;
            }
        }

        lastNodeVisited = pushForwardSegment->getEnd();
        double distance = pushForwardSegment->getLength();

        double parkingHeading = parking->getHeading();

        SG_LOG(SG_AI, SG_BULK, "Creating Pushforward from ID " << pushForwardSegment->getEnd()->getIndex() << " Length : \t" << distance);
        // Add the parking if on first leg and not repeat
        if (waypoints.empty()) {
            pushBackWaypoint(createOnGround(ac, parking->getName(), parking->geod(), dep->getElevation(), vTaxiReduced));
        }
        // Make sure we have at least three WPs
        int numSegments = distance > 15 ? (distance / 5.0) : 3;
        for (int i = 1; i < numSegments; i++) {
            SGGeod pushForwardPt;

            SGGeodesy::direct(parking->geod(), parkingHeading,
                              (((double)i / numSegments) * distance), pushForwardPt, az2);
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "pushforward-%03d", (short)i);
            FGAIWaypoint* wpt = createOnGround(ac, string(buffer), pushForwardPt, dep->getElevation(), vTaxiReduced);

            wpt->setRouteIndex(pushForwardSegment->getIndex());
            pushBackWaypoint(wpt);
        }

        // This will trigger the release of parking
        waypoints.back()->setName(string("PushBackPoint-pushforward"));
    }

    return true;
}
/*******************************************************************
* createPushBackFallBack
* This is the backup function for airports that don't have a
* network yet.
******************************************************************/
void FGAIFlightPlan::createPushBackFallBack(FGAIAircraft* ac, bool firstFlight, FGAirport* dep,
                                            double radius,
                                            const string& fltType,
                                            const string& aircraftType,
                                            const string& airline)
{
    double az2 = 0.0;

    double vTaxi = ac->getPerformance()->vTaxi();
    double vTaxiBackward = vTaxi * (-2.0 / 3.0);
    double vTaxiReduced = vTaxi * (2.0 / 3.0);

    double heading = 180.0; // this is a completely arbitrary heading!
    FGAIWaypoint* wpt = createOnGround(ac, string("park"), dep->geod(), dep->getElevation(), vTaxiBackward);

    pushBackWaypoint(wpt);

    SGGeod coord;
    SGGeodesy::direct(dep->geod(), heading, 10, coord, az2);
    wpt = createOnGround(ac, string("park2"), coord, dep->getElevation(), vTaxiBackward);

    pushBackWaypoint(wpt);

    SGGeodesy::direct(dep->geod(), heading, 2.2 * radius, coord, az2);
    wpt = createOnGround(ac, string("taxiStartFallback"), coord, dep->getElevation(), vTaxiReduced);
    pushBackWaypoint(wpt);
}
