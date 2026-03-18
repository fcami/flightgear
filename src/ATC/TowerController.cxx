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
#include <ATC/TowerController.hxx>
#include <ATC/atc_mgr.hxx>
#include <ATC/trafficcontrol.hxx>

using std::sort;
using std::string;

/***************************************************************************
 * class FGTowerController
 * subclass of FGATCController
 **************************************************************************/

FGTowerController::FGTowerController(FGAirportDynamics* par) : FGATCController()
{
    parent = par;
}

FGTowerController::~FGTowerController()
{
}

//
void FGTowerController::announcePosition(int id,
                                         FGAIFlightPlan* intendedRoute,
                                         int currentPosition, double lat,
                                         double lon, double heading,
                                         double speed, double alt,
                                         double radius, int leg,
                                         FGAIAircraft* ref)
{
    init();

    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    // Add a new TrafficRecord if no one exists for this aircraft.
    if (i == activeTraffic.end() || (activeTraffic.empty())) {
        FGTrafficRecord* rec = new FGTrafficRecord();
        rec->setId(id);
        rec->setPositionAndIntentions(currentPosition, intendedRoute);
        rec->setPositionAndHeading(lat, lon, heading, speed, alt, leg);
        rec->setRunway(intendedRoute->getRunway());
        rec->setLeg(leg);
        rec->setCallsign(ref->getCallSign());
        rec->setRadius(radius);
        rec->setAircraft(ref);
        SGSharedPtr<FGTrafficRecord> sharedRec = static_cast<FGTrafficRecord*>(rec);
        activeTraffic.push_back(sharedRec);

        if (leg <= AILeg::TAKEOFF) {
            // Don't just schedule the aircraft for the tower controller, also assign if to the correct active runway.
            time_t now = globals->get_time_params()->get_cur_time();
            ActiveRunwayQueue* rwy = parent->getRunwayQueue(intendedRoute->getRunway());
            rwy->requestTimeSlot(sharedRec);
            SG_LOG(SG_ATC, SG_DEBUG, ref->getTrafficRef()->getCallSign() << "(" << ref->getID() << ") You are number " << rwy->getrunwayQueueSize() << " for takeoff from " << parent->parent()->getId() << "/" << rwy->getRunwayName() << " " << ref);
            if (airportGroundRadar->add(sharedRec)) {
                SG_LOG(SG_ATC, SG_DEBUG,
                       "Added " << (sharedRec)->getCallsign() << "(" << (sharedRec)->getId() << ") " << sharedRec);

            } else {
                SG_LOG(SG_ATC, SG_DEV_WARN,
                       "Not Added " << (sharedRec)->getCallsign() << "(" << (sharedRec)->getId() << ") " << sharedRec);
            }
        } else if (leg < AILeg::CRUISE) {
            SG_LOG(SG_ATC, SG_DEBUG, ref->getTrafficRef()->getCallSign() << "(" << ref->getID() << ") Goodbye from " << intendedRoute->departureAirport()->getId() << " " << ref->getTrafficRef() << " " << ref);
        } else {
            SG_LOG(SG_ATC, SG_DEBUG, ref->getTrafficRef()->getCallSign() << "(" << ref->getID() << ") Welcome to " << intendedRoute->arrivalAirport()->getId() << " " << ref->getTrafficRef() << " " << ref);
        }
    } else {
        if ((*i)->getLeg() != leg && leg == AILeg::PARKING_TAXI) {
            if (airportGroundRadar->add(*i)) {
                SG_LOG(SG_ATC, SG_DEBUG,
                       "Added " << (*i)->getCallsign() << "(" << (*i)->getId() << ") " << (*i));

            } else {
                SG_LOG(SG_ATC, SG_DEV_WARN,
                       "Not Added " << (*i)->getCallsign() << "(" << (*i)->getId() << ") " << (*i) << " Leg " << (*i)->getLeg() << " " << leg);
            }
        }
        if (((*i)->getLeg() > AILeg::RUNWAY_TAXI) && ((*i)->getLeg() < AILeg::CRUISE ||
                                                      (*i)->getLeg() > AILeg::LANDING)) {
            // We must be on the ground
            bool moved = airportGroundRadar->move(SGRect<double>(lat, lon), *i);
            if (!moved) {
                SG_LOG(SG_ATC, SG_DEV_WARN,
                       "Not moved " << (*i)->getCallsign() << "(" << (*i)->getId() << ")" << *i << " Leg " << (*i)->getLeg());
            }
        }
        (*i)->setPositionAndHeading(lat, lon, heading, speed, alt, leg);
        (*i)->setRunway(intendedRoute->getRunway());
        if ((*i)->getLeg() > AILeg::RUNWAY_TAXI && (*i)->getLeg() < AILeg::CRUISE) {
            ActiveRunwayQueue* rwy = parent->getRunwayQueue(intendedRoute->getRunway());

            auto queuedAcft = rwy->get((*i)->getId());
            if (!queuedAcft) {
                time_t now = globals->get_time_params()->get_cur_time();
                rwy->requestTimeSlot((*i));
                SG_LOG(SG_ATC, SG_DEBUG, ref->getTrafficRef()->getCallSign() << "(" << ref->getID() << ") You are number " << rwy->getrunwayQueueSize() << " for takeoff from " << parent->parent()->getId() << "/" << rwy->getRunwayName() << " " << ref);
            }

            auto blocker = airportGroundRadar->getBlockedBy(*i);
            if (blocker != nullptr) {
                (*i)->setWaitsForId(blocker->getId());
                double distM = SGGeodesy::distanceM((*i)->getPos(), blocker->getPos());
                int newSpeed = blocker->getSpeed() * (distM / 100);
                SG_LOG(SG_ATC, SG_DEBUG,
                       (*i)->getCallsign() << "(" << (*i)->getId() << ") is blocked for takeoff by " << blocker->getCallsign() << "(" << blocker->getId() << ") new speed " << newSpeed << " dist " << distM);
                (*i)->setSpeedAdjustment(newSpeed);
            } else {
                int oldWaitsForId = (*i)->getWaitsForId();
                if (oldWaitsForId > 0) {
                    SG_LOG(SG_ATC, SG_DEBUG,
                           (*i)->getCallsign() << "(" << (*i)->getId() << ") cleared of blocker " << oldWaitsForId);
                    (*i)->setResumeTaxi(true);
                }
                (*i)->clearSpeedAdjustment();
                (*i)->setWaitingSince(0);
                (*i)->setWaitsForId(0);
            }
        }
    }
}

void FGTowerController::updateAircraftInformation(int id, SGGeod geod,
                                                  double heading, double speed, double alt,
                                                  double dt)
{
    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    setDt(getDt() + dt);

    time_t now = globals->get_time_params()->get_cur_time();
    if (i == activeTraffic.end() || (activeTraffic.empty())) {
        SG_LOG(SG_ATC, SG_DEV_WARN,
               "AI error: updating aircraft without traffic record at " << SG_ORIGIN);
        return;
    }

    // Update the position of the current aircraft
    (*i)->setPositionAndHeading(geod.getLatitudeDeg(), geod.getLongitudeDeg(), heading, speed, alt, AILeg::UNKNOWN);

    if ((*i)->getLeg() < AILeg::CRUISE) {
        // see if we already have a clearance record for the currently active runway
        // NOTE: dd. 2011-08-07: Because the active runway has been constructed in the announcePosition function, we may safely assume that is
        // already exists here. So, we can simplify the current code.

        ActiveRunwayQueue* rwy = parent->getRunwayQueue((*i)->getRunway());
        auto firstInQueue = rwy->getFirstAircraftInDepartureQueue();
        rwy->printRunwayQueue();
        if (firstInQueue && firstInQueue->getId() == (*i)->getId()) {
            // We are first either align or clear for takeoff
            switch ((*i)->getState()) {
            case ATCMessageState::ACK_SWITCH_GROUND_TOWER:
                checkTransmissionState(ATCMessageState::ACK_SWITCH_GROUND_TOWER, ATCMessageState::ACK_SWITCH_GROUND_TOWER, i, now, MSG_LINE_UP_RWY, ATC_GROUND_TO_AIR);
                break;
            case ATCMessageState::CLEARED_TAKEOFF:
                if ((*i)->getRunwaySlot() < now) {
                    (*i)->setHoldPosition(false);
                    checkTransmissionState(ATCMessageState::CLEARED_TAKEOFF, ATCMessageState::CLEARED_TAKEOFF, i, now, MSG_ACKNOWLEDGE_CLEARED_FOR_TAKEOFF, ATC_AIR_TO_GROUND);
                    (*i)->setState(ATCMessageState::ACK_CLEARED_TAKEOFF);
                }
                break;
            case ATCMessageState::ACK_CLEARED_TAKEOFF:
                if ((*i)->getRunwaySlot() < now) {
                    (*i)->setHoldPosition(false);
                    checkTransmissionState(ATCMessageState::ACK_CLEARED_TAKEOFF, ATCMessageState::ACK_CLEARED_TAKEOFF, i, now, MSG_ACKNOWLEDGE_CLEARED_FOR_TAKEOFF, ATC_GROUND_TO_AIR);
                    (*i)->setState(ATCMessageState::ANNOUNCE_ARRIVAL);
                }
                break;
            case ATCMessageState::LINE_UP_RUNWAY:
                if ((*i)->getRunwaySlot() < now) {
                    (*i)->setHoldPosition(false);
                    checkTransmissionState(ATCMessageState::LINE_UP_RUNWAY, ATCMessageState::LINE_UP_RUNWAY, i, now, MSG_LINE_UP_RWY, ATC_AIR_TO_GROUND);
                    (*i)->setState(ATCMessageState::ACK_LINE_UP_RUNWAY);
                } else {
                    (*i)->setHoldPosition(true);
                }
                break;
            case ATCMessageState::ACK_LINE_UP_RUNWAY:
                if ((*i)->getRunwaySlot() < now) {
                    (*i)->setHoldPosition(false);
                    checkTransmissionState(ATCMessageState::ACK_LINE_UP_RUNWAY, ATCMessageState::ACK_LINE_UP_RUNWAY, i, now, MSG_ACKNOWLEDGE_LINE_UP_RWY, ATC_GROUND_TO_AIR);
                    (*i)->setState(ATCMessageState::CLEARED_TAKEOFF);
                }
                break;
            default:
                // As long as the first isn't cleared wait
                if ((*i)->getState() < ATCMessageState::CLEARED_TAKEOFF) {
                    (*i)->setHoldPosition(true);
                }
                SG_LOG(SG_ATC, SG_BULK,
                       (*i)->getCallsign() << "(" << (*i)->getId() << ") Waiting for " << ((*i)->getRunwaySlot() - now) << " seconds MsgStatus " << (*i)->getState());
                break;
            }
        } else {
            auto blocker = airportGroundRadar->getBlockedBy(*i);
            if (blocker != nullptr) {
                (*i)->setWaitsForId(blocker->getId());
                double distM = SGGeodesy::distanceM((*i)->getPos(), blocker->getPos());
                int newSpeed = blocker->getSpeed() * (distM / 100);
                SG_LOG(SG_ATC, SG_DEBUG,
                       (*i)->getCallsign() << "(" << (*i)->getId() << ") is blocked for takeoff by " << blocker->getCallsign() << "(" << blocker->getId() << ") new speed " << newSpeed);
                (*i)->setSpeedAdjustment(newSpeed);
            }
        }
    }
}

void FGTowerController::signOff(int id)
{
    // ensure we don't modify activeTraffic during destruction
    if (_isDestroying)
        return;

    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);
    if (i == activeTraffic.end() || (activeTraffic.empty())) {
        SG_LOG(SG_ATC, SG_DEV_WARN,
               "AI error: Aircraft without traffic record is signing off from " << getName() << " list " << activeTraffic.empty());
        return;
    }
    SG_LOG(SG_ATC, SG_BULK, "Signing off " << (*i)->getCallsign() << "(" << id << ") from " << getName() << " Leg : " << (*i)->getLeg());

    if ((*i)->getLeg() <= AILeg::CRUISE) {
        const auto trafficRunway = (*i)->getRunway();
        ActiveRunwayQueue* runwayIt = parent->getRunwayQueue(trafficRunway);

        SG_LOG(SG_ATC, SG_BULK, (*i)->getCallsign() << "(" << (*i)->getId() << ")  Cleared " << id << " from " << runwayIt->getRunwayName() << " cleared " << runwayIt->getCleared());
        runwayIt->removeFromQueue(id);
    } else {
        time_t now = globals->get_time_params()->get_cur_time();
        if (checkTransmissionState(ATCMessageState::NORMAL, ATCMessageState::LANDING_TAXI, i, now, MSG_TAXI_PARK, ATC_GROUND_TO_AIR)) {
            (*i)->setState(ATCMessageState::SWITCH_TOWER_TO_GROUND);
        }
    }

    FGATCController::signOff(id);
}

// Note:
// if we make trafficrecord a member of the base class
// the following three functions: signOff, hasInstruction and getInstruction can
// become devirtualized and be a member of the base ATCController class
// which would simplify code maintenance.
// note that this function is probably obsolete
bool FGTowerController::hasInstruction(int id)
{
    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    if (i == activeTraffic.end() || activeTraffic.empty()) {
        SG_LOG(SG_ATC, SG_DEV_WARN,
               "AI error: checking ATC instruction for aircraft without traffic record at " << SG_ORIGIN);
    } else {
        return (*i)->hasInstruction();
    }
    return false;
}


FGATCInstruction FGTowerController::getInstruction(int id)
{
    // Search activeTraffic for a record matching our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    if (i == activeTraffic.end() || activeTraffic.empty()) {
        SG_LOG(SG_ATC, SG_DEV_WARN,
               "AI error: requesting ATC instruction for aircraft without traffic record at " << SG_ORIGIN);
    } else {
        return (*i)->getInstruction();
    }
    return FGATCInstruction();
}

void FGTowerController::render(bool visible)
{
    // this should be bulk, since its called quite often
    SG_LOG(SG_ATC, SG_BULK, "FGTowerController::render function not yet implemented");
}

string FGTowerController::getName() const
{
    return string(parent->parent()->getName() + "-tower");
}


void FGTowerController::update(double dt)
{
    FGATCController::eraseDeadTraffic();
}

int FGTowerController::getFrequency()
{
    int towerFreq = parent->getTowerFrequency(2);
    return towerFreq;
}
