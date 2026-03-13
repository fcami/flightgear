// SPDX-FileCopyrightText: 2006 Durk Talsma
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Airports/airports_fwd.hxx>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Shape>

#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>
// There is probably a better include than sg_geodesy to get the SG_NM_TO_METER...
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/structure/SGReferenced.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

#include <AIModel/AIConstants.hxx>

class FGAIAircraft;
typedef std::vector<FGAIAircraft*> AircraftVec;
typedef std::vector<FGAIAircraft*>::iterator AircraftVecIterator;

class FGAIFlightPlan;
typedef std::vector<FGAIFlightPlan*> FlightPlanVec;
typedef std::vector<FGAIFlightPlan*>::iterator FlightPlanVecIterator;
typedef std::map<std::string, FlightPlanVec> FlightPlanVecMap;

class FGTrafficRecord;
typedef std::list<SGSharedPtr<FGTrafficRecord>> TrafficVector;
typedef std::list<SGSharedPtr<FGTrafficRecord>>::const_iterator TrafficVectorIterator;

class ActiveRunwayQueue;
typedef std::vector<ActiveRunwayQueue> ActiveRunwayVec;
typedef std::vector<ActiveRunwayQueue>::iterator ActiveRunwayVecIterator;

typedef std::vector<int> intVec;
typedef std::vector<int>::iterator intVecIterator;

/**************************************************************************************
 * class FGATCInstruction
 * like class FGATC Controller, this class definition should go into its own file
 * and or directory... For now, just testing this stuff out though...
 *************************************************************************************/
class FGATCInstruction
{
private:
    bool holdPattern = false;
    int requestedArrivalTime{0};
    bool holdPosition = false;
    bool requestHoldPosition = false;
    bool resumeTaxi = false;
    bool changeSpeed = false;
    bool changeHeading = false;
    bool changeAltitude = false;
    bool resolveCircularWait = false;
    int waitsForId;
    int waitingSince;

    double speed = std::numeric_limits<double>::max();
    double heading;
    double alt;


public:
    FGATCInstruction();

    bool hasInstruction() const;
    bool getHoldPattern() const
    {
        return holdPattern;
    };
    void setRunwaySlot(int val)
    {
        requestedArrivalTime = val;
    };
    int getRunwaySlot() const
    {
        return requestedArrivalTime;
    };
    bool getHoldPosition() const
    {
        return holdPosition;
    };
    bool getRequestHoldPosition() const
    {
        return requestHoldPosition;
    };
    bool getResumeTaxi() const
    {
        return resumeTaxi;
    };

    bool getChangeSpeed() const
    {
        return changeSpeed;
    };
    bool getChangeHeading() const
    {
        return changeHeading;
    };
    bool getChangeAltitude() const
    {
        return changeAltitude;
    };
    bool getCheckForCircularWait() const
    {
        return resolveCircularWait;
    };

    double getSpeed() const
    {
        return speed;
    };
    double getHeading() const
    {
        return heading;
    };
    double getAlt() const
    {
        return alt;
    };

    int getWaitsForId() const
    {
        return waitsForId;
    };
    int getWaitingSince() const
    {
        return waitingSince;
    };

    void setHoldPattern(bool val)
    {
        holdPattern = val;
    };
    void setHoldPosition(bool val)
    {
        holdPosition = val;
    };
    void setRequestHoldPosition(bool val)
    {
        requestHoldPosition = val;
    };
    void setResumeTaxi(bool val)
    {
        resumeTaxi = val;
    };
    void setChangeSpeed(bool val)
    {
        changeSpeed = val;
    };
    void setChangeHeading(bool val)
    {
        changeHeading = val;
    };
    void setChangeAltitude(bool val)
    {
        changeAltitude = val;
    };
    void setResolveCircularWait(bool val)
    {
        resolveCircularWait = val;
    };

    void setSpeed(double val)
    {
        speed = val;
    };
    void setHeading(double val)
    {
        heading = val;
    };
    void setAlt(double val)
    {
        alt = val;
    };

    void setWaitsForId(int id)
    {
        waitsForId = id;
    };
    void setWaitingSince(int t)
    {
        waitingSince = t;
    };
};


/**************************************************************************************
 * class FGTrafficRecord
 * Represents the interaction of an AI Aircraft and ATC
 *************************************************************************************/
class FGTrafficRecord : public SGReferenced
{
private:
    int id;
    int currentPos;
    int leg;
    int frequencyId;
    int state;
    bool allowTransmission;
    bool allowPushback;
    int priority;
    int plannedArrivalTime{0};
    time_t timer;
    intVec intentions;
    FGATCInstruction instruction;
    SGGeod pos;
    double heading;
    double headingDiff;
    double speed;
    double altitude;
    double radius;
    time_t takeOffTimeSlot{0};
    std::string callsign;
    std::string runway; //FIXME departure or arrival runway. not relevant since flights are short
    FGAirportRef departure;
    FGAirportRef arrival;
    SGSharedPtr<FGAIAircraft> aircraft;

public:
    FGTrafficRecord();
    virtual ~FGTrafficRecord();

    void setId(int val)
    {
        id = val;
    };
    void setRadius(double rad)
    {
        radius = rad;
    };
    void setPositionAndIntentions(int pos, FGAIFlightPlan* route);
    void setRunway(const std::string& rwy)
    {
        runway = rwy;
    };
    void setLeg(int lg)
    {
        leg = lg;
    };
    int getId() const
    {
        return id;
    };
    /**
     * Return the current ATC State of type @see ATCMessageState
    */
    int getState() const
    {
        return state;
    };
    /**
     * Set the current ATC State of type @see ATCMessageState
    */
    void setState(int s)
    {
        state = s;
    }
    FGATCInstruction getInstruction() const
    {
        return instruction;
    };
    bool hasInstruction() const
    {
        return instruction.hasInstruction();
    };
    void setTakeOffSlot(time_t timeSlot) { takeOffTimeSlot = timeSlot; };
    time_t getTakeOffSlot() { return takeOffTimeSlot; };

    void setPositionAndHeading(double lat, double lon, double hdg, double spd, double alt, int leg);
    bool checkPositionAndIntentions(FGTrafficRecord& other);
    int crosses(FGGroundNetwork*, FGTrafficRecord& other);
    bool isOpposing(FGGroundNetwork*, FGTrafficRecord& other, int node);

    bool isActive(int margin) const;
    bool isDead() const;
    void clearATCController() const;

    bool onRoute(FGGroundNetwork*, FGTrafficRecord& other);

    bool getSpeedAdjustment() const
    {
        return instruction.getChangeSpeed();
    };
    void setPlannedArrivalTime(int val)
    {
        plannedArrivalTime = val;
    };
    /**Arrival time planned by aircraft.*/
    int getPlannedArrivalTime() const
    {
        return plannedArrivalTime;
    };
    void setRunwaySlot(int val)
    {
        if (plannedArrivalTime) {
            SG_LOG(SG_ATC, SG_BULK, callsign << "(" << id << ") Runwayslot timedelta " << (val - plannedArrivalTime));
        }
        instruction.setRunwaySlot(val);
    };
    /**Arrival time requested by ATC.*/
    int getRunwaySlot()
    {
        return instruction.getRunwaySlot();
    };
    SGGeod getPos()
    {
        return pos;
    }
    double getHeading() const
    {
        return heading;
    };
    /**The last diff of heading when turning.*/
    double getHeadingDiff() const
    {
        return headingDiff;
    };
    double getSpeed() const
    {
        return speed;
    };
    double getFAltitude() const
    {
        return altitude;
    };
    double getRadius() const
    {
        return radius;
    };

    int getWaitsForId() const
    {
        return instruction.getWaitsForId();
    };
    int getWaitingSince() const
    {
        return instruction.getWaitingSince();
    };

    void setSpeedAdjustment(double spd);
    void setHeadingAdjustment(double heading);
    void clearSpeedAdjustment()
    {
        instruction.setChangeSpeed(false);
    };
    void clearHeadingAdjustment()
    {
        instruction.setChangeHeading(false);
    };

    bool hasHeadingAdjustment() const
    {
        return instruction.getChangeHeading();
    };
    bool hasHoldPosition() const
    {
        return instruction.getHoldPosition();
    };
    bool getRequestHoldPosition() const
    {
        return instruction.getRequestHoldPosition();
    };
    bool getResumeTaxi() const
    {
        return instruction.getResumeTaxi();
    };
    void setHoldPosition(bool inst)
    {
        instruction.setHoldPosition(inst);
    };
    void setRequestHoldPosition(bool inst)
    {
        instruction.setRequestHoldPosition(inst);
    };
    void setResumeTaxi(bool inst)
    {
        instruction.setResumeTaxi(inst);
    };
    int getWaitsForId()
    {
        return instruction.getWaitsForId();
    }
    void setWaitsForId(int id)
    {
        instruction.setWaitsForId(id);
    };
    void setWaitingSince(int id)
    {
        instruction.setWaitingSince(id);
    };


    void setResolveCircularWait()
    {
        instruction.setResolveCircularWait(true);
    };
    void clearResolveCircularWait()
    {
        instruction.setResolveCircularWait(false);
    };

    void setCallsign(const std::string& clsgn) { callsign = clsgn; };
    const std::string& getCallsign() const
    {
        return callsign;
    };

    const std::string& getRunway() const
    {
        return runway;
    };


    void setDeparture(const FGAirportRef& dep);
    FGAirportRef getDeparture() const;

    void setArrival(const FGAirportRef& arr);
    FGAirportRef getArrival() const;

    void setAircraft(FGAIAircraft* ref);
    FGAIAircraft* getAircraft() const;

    void updateState()
    {
        state++;
        allowTransmission = true;
    };

    int getTime() const
    {
        return timer;
    };
    int getLeg() const
    {
        return leg;
    };
    void setTime(time_t time)
    {
        timer = time;
    };

    bool pushBackAllowed() const;
    bool allowTransmissions() const
    {
        return allowTransmission;
    };
    void allowPushBack() { allowPushback = true; };
    void denyPushBack() { allowPushback = false; };
    void suppressRepeatedTransmissions()
    {
        allowTransmission = false;
    };
    void allowRepeatedTransmissions()
    {
        allowTransmission = true;
    };
    void nextFrequency()
    {
        frequencyId++;
    };
    int getNextFrequency() const
    {
        return frequencyId;
    };
    intVec& getIntentions()
    {
        return intentions;
    };
    int getCurrentPosition() const
    {
        return currentPos;
    };
    void setPriority(int p) { priority = p; };
    int getPriority() const { return priority; };
};

/***********************************************************************
 * Active runway, a utility class to keep track of which aircraft has
 * clearance for a given runway.
 **********************************************************************/
class ActiveRunwayQueue
{
public:
    /**Separation between aircraft in seconds.*/
    const time_t SEPARATION = 120;

private:
    const std::string icao;
    const std::string rwy;
    int currentlyCleared;
    double distanceToFinal;
    using AircraftRefVec = std::vector<SGSharedPtr<FGTrafficRecord>>;
    AircraftRefVec runwayQueue;

public:
    ActiveRunwayQueue(const std::string& apt, const std::string& r, int cc);

    const std::string& getRunwayName() const
    {
        return rwy;
    };
    /**Get id of cleared AI Aircraft*/
    int getCleared() const
    {
        return currentlyCleared;
    };

    double getApproachDistance() const
    {
        return distanceToFinal;
    };

    //time_t getEstApproachTime() { return estimatedArrival; };

    //void setEstApproachTime(time_t time) { estimatedArrival = time; };
    void addToQueue(SGSharedPtr<FGTrafficRecord> ac);

    void setCleared(int number)
    {
        currentlyCleared = number;
    };
    void requestTimeSlot(SGSharedPtr<FGTrafficRecord> eta);
    void updateFirst(SGSharedPtr<FGTrafficRecord> eta, time_t newETA);
    //time_t requestTimeSlot(time_t eta, std::string wakeCategory);
    int getrunwayQueueSize()
    {
        return runwayQueue.size();
    };

    const SGSharedPtr<FGTrafficRecord> getFirstAircraftInDepartureQueue() const;

    const SGSharedPtr<FGTrafficRecord> getFirstOfStatus(int stat) const;

    const SGSharedPtr<FGTrafficRecord> get(const int id) const;

    void removeFromQueue(const int id);

    bool isQueued(const int id) const;

    void updateDepartureQueue();

    void printRunwayQueue() const;

private:
    void resort()
    {
        std::sort(runwayQueue.begin(), runwayQueue.end(),
                  [](const SGSharedPtr<FGTrafficRecord> a, const SGSharedPtr<FGTrafficRecord> b) {
                      return a->getRunwaySlot() < b->getRunwaySlot();
                  });
    };
};
