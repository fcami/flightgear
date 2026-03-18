// SPDX-FileCopyrightText: 2006 Durk Talsma
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config.h>

#include <algorithm>
#include <cstdio>
#include <random>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Shape>

#include <simgear/scene/material/EffectGeode.hxx>
#include <simgear/scene/material/mat.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Scenery/scenery.hxx>

#include "atc_mgr.hxx"
#include "trafficcontrol.hxx"
#include <AIModel/AIAircraft.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <AIModel/performancedata.hxx>
#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/groundnetwork.hxx>
#include <Radio/radio.hxx>
#include <Traffic/TrafficMgr.hxx>
#include <signal.h>

#include <ATC/ATCController.hxx>
#include <ATC/ApproachController.hxx>
#include <ATC/atc_mgr.hxx>
#include <ATC/trafficcontrol.hxx>

using std::sort;
using std::string;

/***************************************************************************
 * class FGApproachController
 * subclass of FGATCController
 **************************************************************************/

FGApproachController::FGApproachController(FGAirportDynamics* par) : FGATCController()
{
    parent = par;
}

FGApproachController::~FGApproachController()
{
}


void FGApproachController::announcePosition(int id,
                                            FGAIFlightPlan* intendedRoute,
                                            int currentPosition,
                                            double lat, double lon,
                                            double heading, double speed,
                                            double alt, double radius,
                                            int leg, FGAIAircraft* ref)
{
    init();

    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    // Add a new TrafficRecord if no one exists for this aircraft.
    if (i == activeTraffic.end() || activeTraffic.empty()) {
        FGTrafficRecord* rec = new FGTrafficRecord();
        rec->setId(id);
        rec->setPositionAndIntentions(currentPosition, intendedRoute);
        rec->setPositionAndHeading(lat, lon, heading, speed, alt, leg);
        rec->setRunway(intendedRoute->getRunway());
        rec->setLeg(leg);
        rec->setCallsign(ref->getCallSign());
        rec->setAircraft(ref);
        rec->setPlannedArrivalTime(intendedRoute->getArrivalTime());
        activeTraffic.push_back(rec);
    } else {
        (*i)->setRunway(intendedRoute->getRunway());
        (*i)->setPositionAndHeading(lat, lon, heading, speed, alt, leg);
        (*i)->setPlannedArrivalTime(intendedRoute->getArrivalTime());
    }
}

void FGApproachController::updateAircraftInformation(int id, SGGeod geod,
                                                     double heading, double speed, double alt,
                                                     double dt)
{
    time_t now = globals->get_time_params()->get_cur_time();
    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);
    TrafficVectorIterator current;

    // update position of the current aircraft
    if (i == activeTraffic.end() || activeTraffic.empty()) {
        SG_LOG(SG_ATC, SG_DEV_WARN,
               "FGApproachController updating aircraft without traffic record at " << SG_ORIGIN);
    } else {
        (*i)->setPositionAndHeading(geod.getLatitudeDeg(), geod.getLongitudeDeg(), heading, speed, alt, AILeg::UNKNOWN);
        current = i;
        if ((*current)->getAircraft()) {
            //FIXME No call to aircraft! -> set instruction
            time_t time_diff =
                (*current)->getAircraft()->checkForArrivalTime(string("final001"));
            if (time_diff != 0) {
                SG_LOG(SG_ATC, SG_BULK, (*current)->getCallsign() << "|ApproachController: checking for speed " << time_diff);
            }
            if (time_diff > 15) {
                (*current)->setSpeedAdjustment((*current)->getAircraft()->getPerformance()->vDescent() *
                                               1.35);
            } else if (time_diff > 5) {
                (*current)->setSpeedAdjustment((*current)->getAircraft()->getPerformance()->vDescent() *
                                               1.2);
            } else if (time_diff < -15) {
                (*current)->setSpeedAdjustment((*current)->getAircraft()->getPerformance()->vDescent() *
                                               0.65);
            } else if (time_diff < -5) {
                (*current)->setSpeedAdjustment((*current)->getAircraft()->getPerformance()->vDescent() *
                                               0.8);
            } else {
                (*current)->clearSpeedAdjustment();
            }
            if ((now - lastTransmission) > 15) {
                available = true;
            }
            if (checkTransmissionState(ATCMessageState::ACK_ARRIVAL, ATCMessageState::ACK_ARRIVAL, current, now, MSG_ACKNOWLEDGE_ARRIVAL, ATC_GROUND_TO_AIR)) {
                SG_LOG(SG_ATC, SG_DEBUG, (*current)->getCallsign() << "(" << (*current)->getId() << ") " << " Hold " << (*current)->getRunwaySlot() << " " << (*current)->getPlannedArrivalTime() << " " << ((*current)->getRunwaySlot() > (*current)->getPlannedArrivalTime()));
                if ((*current)->getRunwaySlot() > (*current)->getPlannedArrivalTime()) {
                    (*current)->setState(ATCMessageState::HOLD_PATTERN);
                } else {
                    (*current)->setState(ATCMessageState::CLEARED_TO_LAND);
                }
            }
            //Start of our status runimplicit "announce arrival"
            if (checkTransmissionState(ATCMessageState::ANNOUNCE_ARRIVAL, ATCMessageState::ANNOUNCE_ARRIVAL, current, now, MSG_ARRIVAL, ATC_AIR_TO_GROUND)) {
                parent->getRunwayQueue((*current)->getRunway())->requestTimeSlot((*current));
                (*current)->setState(ATCMessageState::ACK_ARRIVAL);
            }
            if (checkTransmissionState(ATCMessageState::HOLD_PATTERN, ATCMessageState::HOLD_PATTERN, current, now, MSG_ACKNOWLEDGE_HOLD, ATC_AIR_TO_GROUND)) {
                (*current)->setState(ATCMessageState::ACK_HOLD);
            }
            if (checkTransmissionState(ATCMessageState::CLEARED_TO_LAND, ATCMessageState::CLEARED_TO_LAND, current, now, MSG_CLEARED_TO_LAND, ATC_GROUND_TO_AIR)) {
                (*current)->setState(ATCMessageState::ACK_CLEARED_TO_LAND);
            }
            if (checkTransmissionState(ATCMessageState::ACK_CLEARED_TO_LAND, ATCMessageState::ACK_CLEARED_TO_LAND, current, now, MSG_ACKNOWLEDGE_CLEARED_TO_LAND, ATC_AIR_TO_GROUND)) {
                (*current)->setState(ATCMessageState::SWITCH_GROUND_TOWER);
            }
            if (checkTransmissionState(ATCMessageState::SWITCH_GROUND_TOWER, ATCMessageState::SWITCH_GROUND_TOWER, current, now, MSG_SWITCH_TOWER_FREQUENCY, ATC_GROUND_TO_AIR)) {
            }
            if (checkTransmissionState(ATCMessageState::ACK_SWITCH_GROUND_TOWER, ATCMessageState::ACK_SWITCH_GROUND_TOWER, current, now, MSG_ACKNOWLEDGE_SWITCH_TOWER_FREQUENCY, ATC_AIR_TO_GROUND)) {
                (*current)->setState(ATCMessageState::LANDING_TAXI);
            }
        }
        //(*current)->setSpeedAdjustment((*current)->getAircraft()->getPerformance()->vDescent() + time_diff);
    }
    setDt(getDt() + dt);
}

/** Periodically check for and remove dead traffic records */
void FGApproachController::update(double dt)
{
    FGATCController::eraseDeadTraffic();
}

void FGApproachController::render(bool visible)
{
    // Must be BULK in order to prevent it being called each frame
    if (visible) {
        SG_LOG(SG_ATC, SG_BULK, "FGApproachController::render function not yet implemented");
    }
}

string FGApproachController::getName() const
{
    return string(parent->parent()->getName() + "-approach");
}

int FGApproachController::getFrequency()
{
    int groundFreq = parent->getApproachFrequency(2);
    int towerFreq = parent->getTowerFrequency(2);
    return groundFreq > 0 ? groundFreq : towerFreq;
}
