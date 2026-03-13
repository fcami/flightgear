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

#include <simgear/math/sg_geodesy.hxx>
#include <simgear/scene/material/EffectGeode.hxx>
#include <simgear/scene/material/mat.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Scenery/scenery.hxx>

#include "trafficcontrol.hxx"
#include <AIModel/AIAircraft.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <AIModel/performancedata.hxx>
#include <ATC/atc_mgr.hxx>
#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/groundnetwork.hxx>
#include <Radio/radio.hxx>
#include <Traffic/TrafficMgr.hxx>
#include <signal.h>

using std::sort;
using std::string;

/***************************************************************************
 * ActiveRunway
 **************************************************************************/

ActiveRunwayQueue::ActiveRunwayQueue(const std::string& apt, const std::string& r, int cc) : icao(apt), rwy(r)
{
    SG_LOG(SG_ATC, SG_DEBUG, "ActiveRunway " << icao << "/" << r << " " << cc);
    currentlyCleared = cc;
    distanceToFinal = 6.0 * SG_NM_TO_METER;
};

void ActiveRunwayQueue::removeFromQueue(int id)
{
    SG_LOG(SG_ATC, SG_DEBUG, "Removed from RunwayQueue " << rwy << " " << id);
    auto it = std::find_if(runwayQueue.begin(), runwayQueue.end(), [id](const SGSharedPtr<FGTrafficRecord>& acft) {
        return acft->getId() == id;
    });
    if (it == runwayQueue.end()) {
        SG_LOG(SG_ATC, SG_WARN, "Erasing non existent aircraft " << rwy << " " << id);
        printRunwayQueue();
        return;
    }
    runwayQueue.erase(it);
    setCleared(0);
}

bool ActiveRunwayQueue::isQueued(const int id) const
{
    auto it = std::find_if(runwayQueue.begin(), runwayQueue.end(), [id](const SGSharedPtr<FGTrafficRecord>& acft) {
        return acft->getId() == id;
    });
    if (it == runwayQueue.end()) {
        return false;
    }
    return true;
}

void ActiveRunwayQueue::updateDepartureQueue()
{
    SG_LOG(SG_ATC, SG_WARN, "updateDepartureQueue " << runwayQueue.size());
    runwayQueue.erase(runwayQueue.begin());
    SG_LOG(SG_ATC, SG_WARN, "updateDepartureQueue " << runwayQueue.size());
}

/**
* Fetch next slot for the active runway.
* @param trafficRecord traffic record containing the arrival time
* @return newEta: next slot available; starts at eta parameter
* and adds SEPARATION as needed
*/
void ActiveRunwayQueue::requestTimeSlot(SGSharedPtr<FGTrafficRecord> trafficRecord)
{
    time_t eta = trafficRecord->getPlannedArrivalTime();
    time_t newEta = 0;

    if (get(trafficRecord->getId()) == nullptr) {
        // Push to end. We will resort
        runwayQueue.push_back(trafficRecord);
    }


    // if the aircraft is the first arrival, add to the vector and return eta directly
    if (runwayQueue.empty()) {
        newEta = eta;
        SG_LOG(SG_ATC, SG_DEBUG, icao << "/" << getRunwayName() << " Checked eta slots, using " << eta << " for " << trafficRecord->getCallsign());
    } else {
        // First check the already assigned slots to see where we need to fit the flight in
        SG_LOG(SG_ATC, SG_DEBUG, icao << "/" << getRunwayName() << " Checking eta slots " << eta << " : " << runwayQueue.size() << " Timediff : " << (eta - globals->get_time_params()->get_cur_time()));

        std::vector<SGSharedPtr<FGTrafficRecord>>::iterator i;
        // if the flight is before the first scheduled slot + SEPARATION
        i = runwayQueue.begin();
        if ((eta + SEPARATION) < (*i)->getPlannedArrivalTime()) {
            newEta = eta;
            SG_LOG(SG_ATC, SG_DEBUG, "Added to start. New ETA : " << newEta);
            trafficRecord->setRunwaySlot(newEta);
            resort();
            printRunwayQueue();
            return;
        }

        // else, look through the rest of the slots
        bool found = false;
        while ((i != runwayQueue.end()) && (!found)) {
            std::vector<SGSharedPtr<FGTrafficRecord>>::iterator j = i + 1;

            // if the flight is after the last scheduled slot check if SEPARATION is needed
            if (j == runwayQueue.end()) {
                if (((*i)->getPlannedArrivalTime() + SEPARATION) < eta) {
                    SG_LOG(SG_ATC, SG_DEBUG, "Storing at end");
                    newEta = eta;
                } else {
                    newEta = (*i)->getPlannedArrivalTime() + SEPARATION;
                    SG_LOG(SG_ATC, SG_DEBUG, "Storing at end + SEPARATION");
                }
                SG_LOG(SG_ATC, SG_DEBUG, "End. New ETA : " << newEta << " Timediff : " << (newEta - eta));
                trafficRecord->setRunwaySlot(newEta);
                resort();
                printRunwayQueue();
                return;
            } else {
                // potential slot found
                // check the distance between the previous and next slots
                // distance must be greater than 2* SEPARATION
                if ((((*j)->getPlannedArrivalTime() - (*i)->getPlannedArrivalTime()) > (SEPARATION * 2))) {
                    // now check whether this slot is usable:
                    // eta should fall between the two points
                    // i.e. eta > i AND eta < j
                    SG_LOG(SG_ATC, SG_DEBUG, "Found potential slot after " << (*i));
                    if (eta > (*i)->getPlannedArrivalTime() && (eta < (*j)->getPlannedArrivalTime())) {
                        found = true;
                        if (eta < ((*i)->getPlannedArrivalTime() + SEPARATION)) {
                            newEta = (*i)->getPlannedArrivalTime() + SEPARATION;
                            SG_LOG(SG_ATC, SG_DEBUG, "Using  original" << (*i)->getPlannedArrivalTime() << " + SEPARATION ");
                        } else {
                            newEta = eta;
                            SG_LOG(SG_ATC, SG_DEBUG, "Using original after " << (*i)->getPlannedArrivalTime());
                        }
                    } else if (eta < (*i)->getPlannedArrivalTime()) {
                        found = true;
                        newEta = (*i)->getPlannedArrivalTime() + SEPARATION;
                        SG_LOG(SG_ATC, SG_DEBUG, "Using delayed slot after " << (*i)->getPlannedArrivalTime());
                    }
                    /*
                       if (((*j) - SEPARATION) < eta) {
                       found = true;
                       if (((*i) + SEPARATION) < eta) {
                       newEta = eta;
                       SG_LOG(SG_ATC, SG_BULK, "Using original after " << (*i));
                       } else {
                       newEta = (*i) + SEPARATION;
                       SG_LOG(SG_ATC, SG_BULK, "Using  " << (*i) << " + SEPARATION ");
                       }
                       } */
                }
            }
            ++i;
        }
    }

    SG_LOG(SG_ATC, SG_DEBUG, "Done. New ETA : " << newEta << " " << rwy << " Size : " << runwayQueue.size() << " " << trafficRecord->getCallsign());
    trafficRecord->setRunwaySlot(newEta);
    resort();
    printRunwayQueue();
}

/**
* Update the first and move all records backwards.
* @param trafficRecord traffic record containing the arrival time
* @return newEta: next slot available; starts at eta parameter
* and adds SEPARATION as needed
*/
void ActiveRunwayQueue::updateFirst(SGSharedPtr<FGTrafficRecord> trafficRecord, time_t newETA)
{
    time_t eta = trafficRecord->getPlannedArrivalTime();
    time_t now = globals->get_time_params()->get_cur_time();

    newETA = std::max(newETA, now);

    SG_LOG(SG_ATC, SG_DEBUG, "Update " << trafficRecord->getCallsign() << eta << " " << newETA << " " << now << " " << rwy << " Leg " << trafficRecord->getLeg() << " Size : " << runwayQueue.size() << " ");

    time_t diff = 0;

    for (SGSharedPtr<FGTrafficRecord> queueRecord : runwayQueue) {
        if (trafficRecord->getId() == queueRecord->getId()) {
            diff = newETA - eta;
            diff = std::max((time_t)0, diff);
            SG_LOG(SG_ATC, SG_DEBUG, queueRecord->getCallsign() << "(" << queueRecord->getId() << ")" << " Diff " << diff);
            trafficRecord->setPlannedArrivalTime(newETA);
            queueRecord->setRunwaySlot(queueRecord->getRunwaySlot() + diff);
        } else {
            queueRecord->setRunwaySlot(queueRecord->getRunwaySlot() + diff);
            SG_LOG(SG_ATC, SG_DEBUG, queueRecord->getCallsign() << "(" << queueRecord->getId() << ")" << " Diff " << diff);
        }
    }
    printRunwayQueue();
}

/** Output the contents of the departure queue vector nicely formatted*/
void ActiveRunwayQueue::printRunwayQueue() const
{
    time_t now = globals->get_time_params()->get_cur_time();

    SG_LOG(SG_ATC, SG_DEBUG, "Runway Queue for " << icao << "/" << rwy << " Size : " << runwayQueue.size());
    for (auto acft : runwayQueue) {
        SG_LOG(SG_ATC, SG_DEBUG, " " << acft->getCallsign() << "(" << acft->getId() << ") Leg : " << acft->getLeg() << " MessageState : " << acft->getState() << " Diff : " << acft->getRunwaySlot() - now << " " << acft->getRunwaySlot() << " " << acft->getPlannedArrivalTime() << " Lat : " << acft->getPos().getLatitudeDeg() << " Lon : " << acft->getPos().getLongitudeDeg() << " Speed " << acft->getSpeed() << " Elevation " << acft->getPos().getElevationM());
    }
}

/** Fetch the first aircraft in the departure queue with id */
const SGSharedPtr<FGTrafficRecord> ActiveRunwayQueue::get(const int id) const
{
    auto it = std::find_if(runwayQueue.begin(), runwayQueue.end(), [id](const SGSharedPtr<FGTrafficRecord> acft) {
        return acft->getId() == id;
    });

    if (it == runwayQueue.end()) {
        return nullptr;
    }

    return *it;
}

/** Fetch the first aircraft in the departure queue with a certain status */
const SGSharedPtr<FGTrafficRecord> ActiveRunwayQueue::getFirstOfStatus(int msgStatus) const
{
    auto it = std::find_if(runwayQueue.begin(), runwayQueue.end(), [msgStatus](const SGSharedPtr<FGTrafficRecord> acft) {
        return acft->getState() == msgStatus;
    });

    if (it == runwayQueue.end()) {
        return {};
    }

    return *it;
}

const SGSharedPtr<FGTrafficRecord> ActiveRunwayQueue::getFirstAircraftInDepartureQueue() const
{
    // printRunwayQueue();
    if (runwayQueue.empty()) {
        return nullptr;
    }

    return *runwayQueue.begin();
};

void ActiveRunwayQueue::addToQueue(SGSharedPtr<FGTrafficRecord> ac)
{
    assert(ac);
    assert(!ac->getAircraft());
    assert(!ac->getAircraft()->getDie());
    runwayQueue.push_back(std::move(ac));
    printRunwayQueue();
};


/***************************************************************************
 * FGTrafficRecord
 **************************************************************************/

FGTrafficRecord::FGTrafficRecord() : id(0),
                                     currentPos(0),
                                     leg(0),
                                     frequencyId(0),
                                     state(0),
                                     allowTransmission(true),
                                     allowPushback(true),
                                     priority(0),
                                     timer(0),
                                     heading(0), speed(0), altitude(0), radius(0)
{
}

FGTrafficRecord::~FGTrafficRecord()
{
}

void FGTrafficRecord::setDeparture(const FGAirportRef& dep)
{
    departure = dep;
}

FGAirportRef FGTrafficRecord::getDeparture() const
{
    return departure;
}


void FGTrafficRecord::setArrival(const FGAirportRef& arr)
{
    arrival = arr;
}

FGAirportRef FGTrafficRecord::getArrival() const
{
    return arrival;
}

void FGTrafficRecord::setPositionAndIntentions(int pos,
                                               FGAIFlightPlan* route)
{
    SG_LOG(SG_AI, SG_BULK, "Traffic record position: " << pos);
    currentPos = pos;
    if (runway == "" && route) {
        setRunway(route->getRunway());
    }
    if (!getDeparture()) {
        setDeparture(route->departureAirport());
    }
    if (!getArrival()) {
        setArrival(route->arrivalAirport());
    }
}

void FGTrafficRecord::setAircraft(FGAIAircraft* ref)
{
    aircraft = ref;
}

bool FGTrafficRecord::isDead() const
{
    if (!aircraft) {
        return true;
    }
    return aircraft->getDie();
}

void FGTrafficRecord::clearATCController() const
{
    if (aircraft) {
        aircraft->clearATCController();
    }
}

FGAIAircraft* FGTrafficRecord::getAircraft() const
{
    if (aircraft.valid()) {
        return aircraft.ptr();
    }
    return 0;
}
/**
* Check if another aircraft is ahead of the current one, and on the same taxiway
* @return true / false if this is/isn't the case.
*/
bool FGTrafficRecord::checkPositionAndIntentions(FGTrafficRecord& other)
{
    bool result = false;
    SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| checkPositionAndIntentions CurrentPos : " << currentPos << " Other : " << other.currentPos << " Leg : " << leg << " Other Leg : " << other.leg);
    if (currentPos == other.currentPos && getId() != other.getId()) {
        SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| Check Position and intentions: " << other.getCallsign() << " we are on the same taxiway; Index = " << currentPos);
        int headingTowards = SGGeodesy::courseDeg(other.getPos(), getPos());
        int headingDiff = SGMiscd::normalizePeriodic(-180, 180, headingTowards - getHeading());
        SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| " << heading << "\t" << headingTowards << "\t" << headingDiff);
        // getHeading()
        result = abs(headingDiff) < 89;
    }
    //  else if (! other.intentions.empty())
    //     {
    //       SG_LOG(SG_ATC, SG_BULK, "Start check 2");
    //       intVecIterator i = other.intentions.begin();
    //       while (!((i == other.intentions.end()) || ((*i) == currentPos)))
    //     i++;
    //       if (i != other.intentions.end()) {
    //    SG_LOG(SG_ATC, SG_BULK, "Check Position and intentions: current matches other.intentions");
    //     result = true;
    //       }
    else if (!intentions.empty()) {
        SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| Itentions Size " << intentions.size());
        intVecIterator i = intentions.begin();
        //while (!((i == intentions.end()) || ((*i) == other.currentPos)))
        while (i != intentions.end()) {
            if ((*i) == other.currentPos) {
                break;
            }
            ++i;
        }
        if (i != intentions.end()) {
            SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| Check Position and intentions: " << other.getCallsign() << " matches Index = " << (*i));
            int headingTowards = SGGeodesy::courseDeg(other.getPos(), getPos());
            int distanceM = SGGeodesy::distanceM(other.getPos(), getPos());
            int headingDiff = SGMiscd::normalizePeriodic(-180, 180, headingTowards - getHeading());
            SG_LOG(SG_ATC, SG_BULK, getCallsign() << "| Heading : " << heading << "\t Heading Other->Current" << headingTowards << "\t Heading Diff :" << headingDiff << "\t Distance : " << distanceM);
            // difference of heading is small and it's actually near
            result = abs(headingDiff) < 89 && distanceM < 400;
            //            result = true;
        }
    }
    return result;
}

void FGTrafficRecord::setPositionAndHeading(double lat, double lon,
                                            double hdg, double spd,
                                            double alt, int leg)
{
    this->pos = SGGeod::fromDegFt(lon, lat, alt);
    if (heading != 0 && spd != 0) {
        headingDiff = SGMiscd::normalizePeriodic(-180, 180, heading - hdg);
    } else {
        headingDiff = 0;
    }
    heading = hdg;
    speed = spd;
    altitude = alt;
    if (leg > AILeg::UNKNOWN) {
        this->setLeg(leg);
    }
}

int FGTrafficRecord::crosses(FGGroundNetwork* net,
                             FGTrafficRecord& other)
{
    if (checkPositionAndIntentions(other) || (other.checkPositionAndIntentions(*this)))
        return -1;
    intVecIterator i, j;
    int currentTargetNode = 0, otherTargetNode = 0;
    if (currentPos > 0)
        currentTargetNode = net->findSegment(currentPos)->getEnd()->getIndex(); // OKAY,...
    if (other.currentPos > 0)
        otherTargetNode = net->findSegment(other.currentPos)->getEnd()->getIndex(); // OKAY,...
    if ((currentTargetNode == otherTargetNode) && currentTargetNode > 0)
        return currentTargetNode;
    if (!intentions.empty()) {
        for (i = intentions.begin(); i != intentions.end(); ++i) {
            if ((*i) > 0) {
                if (currentTargetNode ==
                    net->findSegment(*i)->getEnd()->getIndex()) {
                    SG_LOG(SG_ATC, SG_BULK, "Current crosses at " << currentTargetNode);
                    return currentTargetNode;
                }
            }
        }
    }
    if (!other.intentions.empty()) {
        for (i = other.intentions.begin(); i != other.intentions.end(); ++i) {
            if ((*i) > 0) {
                if (otherTargetNode ==
                    net->findSegment(*i)->getEnd()->getIndex()) {
                    SG_LOG(SG_ATC, SG_BULK, "Other crosses at " << currentTargetNode);
                    return otherTargetNode;
                }
            }
        }
    }
    if (!intentions.empty() && !other.intentions.empty()) {
        for (i = intentions.begin(); i != intentions.end(); ++i) {
            for (j = other.intentions.begin(); j != other.intentions.end(); ++j) {
                SG_LOG(SG_ATC, SG_BULK, "finding segment " << *i << " and " << *j);
                if (((*i) > 0) && ((*j) > 0)) {
                    currentTargetNode =
                        net->findSegment(*i)->getEnd()->getIndex();
                    otherTargetNode =
                        net->findSegment(*j)->getEnd()->getIndex();
                    if (currentTargetNode == otherTargetNode) {
                        SG_LOG(SG_ATC, SG_BULK, "Routes will cross at " << currentTargetNode);
                        return currentTargetNode;
                    }
                }
            }
        }
    }
    return -1;
}

bool FGTrafficRecord::onRoute(FGGroundNetwork* net,
                              FGTrafficRecord& other)
{
    int node = -1, othernode = -1;
    if (currentPos > 0)
        node = net->findSegment(currentPos)->getEnd()->getIndex();
    if (other.currentPos > 0)
        othernode =
            net->findSegment(other.currentPos)->getEnd()->getIndex();
    if ((node == othernode) && (node != -1))
        return true;
    if (!other.intentions.empty()) {
        for (intVecIterator i = other.intentions.begin();
             i != other.intentions.end(); ++i) {
            if (*i > 0) {
                othernode = net->findSegment(*i)->getEnd()->getIndex();
                if ((node == othernode) && (node > -1))
                    return true;
            }
        }
    }
    //if (other.currentPos > 0)
    //  othernode = net->findSegment(other.currentPos)->getEnd()->getIndex();
    //if (! intentions.empty())
    //  {
    //    for (intVecIterator i = intentions.begin(); i != intentions.end(); i++)
    //    {
    //      if (*i > 0)
    //        {
    //          node = net->findSegment(*i)->getEnd()->getIndex();
    //          if ((node == othernode) && (node > -1))
    //            return true;
    //        }
    //    }
    //  }
    return false;
}


bool FGTrafficRecord::isOpposing(FGGroundNetwork* net,
                                 FGTrafficRecord& other, int node)
{
    // Check if current segment is the reverse segment for the other aircraft
    SG_LOG(SG_ATC, SG_BULK, "Current segment " << currentPos);

    if ((currentPos > 0) && (other.currentPos > 0)) {
        FGTaxiSegment* opp = net->findSegment(currentPos)->opposite();
        if (opp) {
            if (opp->getIndex() == other.currentPos)
                return true;
        }

        for (intVecIterator i = intentions.begin(); i != intentions.end(); ++i) {
            if ((opp = net->findSegment(other.currentPos)->opposite())) {
                if ((*i) > 0)
                    if (opp->getIndex() ==
                        net->findSegment(*i)->getIndex()) {
                        if (net->findSegment(*i)->getStart()->getIndex() ==
                            node) {
                            {
                                SG_LOG(SG_ATC, SG_BULK, "Found the node " << node);
                                return true;
                            }
                        }
                    }
            }
            if (!other.intentions.empty()) {
                for (intVecIterator j = other.intentions.begin();
                     j != other.intentions.end(); ++j) {
                    SG_LOG(SG_ATC, SG_BULK, "Current segment 1 " << (*i));
                    if ((*i) > 0) {
                        if ((opp = net->findSegment(*i)->opposite())) {
                            if (opp->getIndex() ==
                                net->findSegment(*j)->getIndex()) {
                                SG_LOG(SG_ATC, SG_BULK, "Nodes " << net->findSegment(*i)->getIndex() << " and  " << net->findSegment(*j)->getIndex() << " are opposites ");
                                if (net->findSegment(*i)->getStart()->getIndex() == node) {
                                    {
                                        SG_LOG(SG_ATC, SG_BULK, "Found the node " << node);
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool FGTrafficRecord::isActive(int margin) const
{
    if (aircraft->getDie()) {
        return false;
    }

    time_t now = globals->get_time_params()->get_cur_time();
    time_t deptime = aircraft->getTrafficRef()->getDepartureTime();
    return ((now + margin) > deptime);
}


void FGTrafficRecord::setSpeedAdjustment(double spd)
{
    instruction.setChangeSpeed(true);
    instruction.setSpeed(spd);
}

void FGTrafficRecord::setHeadingAdjustment(double heading)
{
    instruction.setChangeHeading(true);
    instruction.setHeading(heading);
}

bool FGTrafficRecord::pushBackAllowed() const
{
    return allowPushback;
}


/***************************************************************************
 * FGATCInstruction
 *
 **************************************************************************/

FGATCInstruction::FGATCInstruction()
{
    holdPattern = false;
    holdPosition = false;
    changeSpeed = false;
    changeHeading = false;
    changeAltitude = false;
    resolveCircularWait = false;

    speed = 0;
    heading = 0;
    alt = 0;
    waitsForId = 0;
}

bool FGATCInstruction::hasInstruction() const
{
    return (holdPattern || holdPosition || changeSpeed || changeHeading || changeAltitude || resolveCircularWait);
}
