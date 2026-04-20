/*
 * SPDX-FileName: groundnet.cxx
 * SPDX-FileComment: Implementation of the FlightGear airport ground handling code
 * SPDX-FileCopyrightText: 2004 Durk Talsma
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>

#include <simgear/debug/logstream.hxx>
#include <simgear/math/SGGeometryFwd.hxx>
#include <simgear/math/SGIntersect.hxx>
#include <simgear/math/SGLineSegment.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/timing/timestamp.hxx>

#include <Airports/airport.hxx>
#include <Airports/runways.hxx>
#include <Scenery/scenery.hxx>

#include "groundnetwork.hxx"

using std::string;


/***************************************************************************
 * FGTaxiSegment
 **************************************************************************/

FGTaxiSegment::FGTaxiSegment(FGTaxiNode* aStart, FGTaxiNode* aEnd) : startNode(aStart),
                                                                     endNode(aEnd),
                                                                     isActive(0),
                                                                     index(0),
                                                                     oppositeDirection(0)
{
    if (!aStart || !aEnd) {
        throw sg_exception("Missing node arguments creating FGTaxiSegment");
    }
}

SGGeod FGTaxiSegment::getCenter() const
{
    FGTaxiNode *start(getStart()), *end(getEnd());
    double heading, length, az2;
    SGGeodesy::inverse(start->geod(), end->geod(), heading, az2, length);
    return SGGeodesy::direct(start->geod(), heading, length * 0.5);
}

FGTaxiNodeRef FGTaxiSegment::getEnd() const
{
    return const_cast<FGTaxiNode*>(endNode);
}

FGTaxiNodeRef FGTaxiSegment::getStart() const
{
    return const_cast<FGTaxiNode*>(startNode);
}

double FGTaxiSegment::getLength() const
{
    return dist(getStart()->cart(), getEnd()->cart());
}

double FGTaxiSegment::getHeading() const
{
    return SGGeodesy::courseDeg(getStart()->geod(), getEnd()->geod());
}

void FGTaxiSegment::block(int id, time_t blockTime, time_t now)
{
    BlockListIterator i = blockTimes.begin();
    while (i != blockTimes.end()) {
        if (i->getId() == id)
            break;
        i++;
    }
    if (i == blockTimes.end()) {
        blockTimes.push_back(Block(id, blockTime, now));
        sort(blockTimes.begin(), blockTimes.end());
    } else {
        i->updateTimeStamps(blockTime, now);
    }
}

// The segment has a block if any of the block times listed in the block list is
// smaller than the current time.
bool FGTaxiSegment::hasBlock(time_t now)
{
    for (BlockListIterator i = blockTimes.begin(); i != blockTimes.end(); i++) {
        if (i->getBlockTime() < now)
            return true;
    }
    return false;
}

void FGTaxiSegment::unblock(time_t now)
{
    if (blockTimes.empty())
        return;

    if (blockTimes.front().getTimeStamp() < (now - 30)) {
        blockTimes.erase(blockTimes.begin());
    }
}

/***************************************************************************
 * FGTaxiRoute
 **************************************************************************/

/**
  * @param nds The FGTaxiNodes
  * @param rts The ids of the nodes
  * @param dist The length of this FGTaxiRoute in m
  * @param score The score it achieved. If greater than length then there were penalties.
  */

FGTaxiRoute::FGTaxiRoute(const FGTaxiNodeVector& nds, const intVec& rts, double dist, double score, int /* dpth */) : nodes(nds),
                                                                                                                      routes(rts),
                                                                                                                      distance(dist),
                                                                                                                      score(score)
{
    currNode = nodes.begin();
    currRoute = routes.begin();

    if (nodes.size() != (routes.size()) + 1) {
        SG_LOG(SG_GENERAL, SG_ALERT, "ALERT: Misconfigured TaxiRoute : " << nodes.size() << " " << routes.size());
    }
};

bool FGTaxiRoute::next(FGTaxiNodeRef& node, int* rte)
{
    if (nodes.size() != (routes.size()) + 1) {
        throw sg_range_exception("Misconfigured taxi route");
    }

    if (currNode == nodes.end())
        return false;
    node = *(currNode);
    if (currNode != nodes.begin()) {
        // work-around for FLIGHTGEAR-NJN: throw an exception here
        // instead of crashing, to aid debugging
        if (currRoute == routes.end()) {
            throw sg_range_exception("Misconfigured taxi route");
        }

        *rte = *(currRoute);
        ++currRoute;
    } else {
        if (currRoute == routes.end()) {
            throw sg_range_exception("Misconfigured taxi route");
        }
        // Handle special case for the first node.
        *rte = -1 * *(currRoute);
    }
    ++currNode;
    return true;
};

/***************************************************************************
 * FGGroundNetwork()
 **************************************************************************/

FGGroundNetwork::FGGroundNetwork(FGAirport* airport) : parent(airport)
{
    hasNetwork = false;
    version = 0;
    networkInitialized = false;
}

FGGroundNetwork::~FGGroundNetwork()
{
    for (auto seg : segments) {
        delete seg;
    }
}

/**
 * Postprocess the ground network
 * * join forward/backward segments
 * * add penalties for crossing runway
 */

void FGGroundNetwork::init()
{
    if (networkInitialized) {
        SG_LOG(SG_GENERAL, SG_WARN, "duplicate ground-network init");
        return;
    }

    hasNetwork = true;
    int index = 1;

    auto rwys = parent->getRunwaysWithoutReciprocals();
    // establish pairing of segments
    for (auto segment : segments) {
        //TODO Add Scanning for possible hold points
        // Calculate the intersection with runways and add a penalty
        SG_LOG(SG_AI, SG_BULK, "Scanning Segment " << segment->getIndex() << " " << segment->getStart()->geod() << " " << segment->getEnd()->geod() << " Size " << segments.size());
        for (FGRunwayRef rwy : rwys) {
            SG_LOG(SG_AI, SG_BULK, "Scanning Runway " << parent->ident() << "/" << rwy->ident());
            SGRectd pavement = rwy->getRect();
            double lateralOffset = rwy->widthM() / 2;

            const SGGeod leftStart = rwy->pointOffCenterline(0.0, -lateralOffset);
            const SGGeod rightStart = rwy->pointOffCenterline(0.0, lateralOffset);
            const SGGeod leftEnd = rwy->pointOffCenterline(rwy->lengthFt(), -lateralOffset);
            const SGGeod rightEnd = rwy->pointOffCenterline(rwy->lengthFt(), lateralOffset);
            SGGeod r1;
            SGGeod r2;
            auto intersectLeft = SGGeodesy::intersection(leftStart, leftEnd,
                                                         segment->getStart()->geod(), segment->getEnd()->geod());
            auto intersectRight = SGGeodesy::intersection(rightStart, rightEnd,
                                                          segment->getStart()->geod(), segment->getEnd()->geod());
            double segmentLen = segment->getLength();

            if (intersectLeft.has_value()) {
                double len1 = SGGeodesy::distanceM(segment->getStart()->geod(), (*intersectLeft));
                double len2 = SGGeodesy::distanceM(segment->getEnd()->geod(), (*intersectLeft));
                if (len1 + len2 <= segmentLen + 1) {
                    // We add 1m to catch the difference of calculation
                    // The intersection point must be within segment
                    segment->setPenalty(segment->getPenalty() + 100);
                    SG_LOG(SG_AI, SG_BULK, "Intersection Runway " << rwy->ident() << " Parent " << parent->ident() << " " << pavement << " Len 1 " << len1 << " Len 2 " << len2 << " segmentLen " << segmentLen);
                }
            } else {
                SG_LOG(SG_AI, SG_BULK, "No Intersection Runway " << rwy->ident() << " Parent " << parent->ident() << " " << pavement);
            }
            if (intersectRight.has_value()) {
                double len1 = SGGeodesy::distanceM(segment->getStart()->geod(), (*intersectRight));
                double len2 = SGGeodesy::distanceM(segment->getEnd()->geod(), (*intersectRight));
                if (len1 + len2 <= segmentLen + 1) {
                    // We add 1m to catch the difference of calculation
                    // The intersection point must be within segment
                    segment->setPenalty(segment->getPenalty() + 100);
                    SG_LOG(SG_AI, SG_BULK, "Intersection Runway " << rwy->ident() << " Parent " << parent->ident() << " " << pavement << " Len 1 " << len1 << " Len 2 " << len2 << " segmentLen " << segmentLen);
                }
            } else {
                SG_LOG(SG_AI, SG_BULK, "No Intersection Runway " << rwy->ident() << " Parent " << parent->ident() << " " << pavement);
            }
        }

        segment->setIndex(index++);

        if (segment->oppositeDirection) {
            continue; // already established
        }

        FGTaxiSegment* opp = findSegment(segment->endNode, segment->startNode);
        if (opp) {
            assert(opp->oppositeDirection == NULL);
            segment->oppositeDirection = opp;
            opp->oppositeDirection = segment;
        }

        // establish node -> segment end cache
        m_segmentsEndingAtNodeMap.insert(NodeFromSegmentMap::value_type{segment->getEnd(), segment});
    }

    SG_LOG(SG_AI, SG_BULK, "Loaded " << parent->ident());
    networkInitialized = true;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNode(const SGGeod& aGeod) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result;

    FGTaxiNodeVector::const_iterator it;
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            d = localDistanceSqr;
            result = *it;
        }
    }

    return result;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNodeOffRunway(const SGGeod& aGeod, FGRunway* rwy, double marginM) const
{
    FGTaxiNodeVector nodes;
    const SGLineSegmentd runwayLine(rwy->cart(), SGVec3d::fromGeod(rwy->end()));
    const double marginMSqr = marginM * marginM;
    const SGVec3d cartPos = SGVec3d::fromGeod(aGeod);

    std::copy_if(m_nodes.begin(), m_nodes.end(), std::back_inserter(nodes),
                 [runwayLine, cartPos, marginMSqr](const FGTaxiNodeRef& a) {
                     if (a->getIsOnRunway()) return false;

                     // exclude parking positions from consideration. This helps to
                     // exclude airports whose ground nets only list parking positions,
                     // since these typically produce bad results. See discussion in
                     // https://sourceforge.net/p/flightgear/codetickets/2110/
                     if (a->type() == FGPositioned::PARKING) return false;

                     return (distSqr(runwayLine, a->cart()) >= marginMSqr);
                 });


    // find closest of matching nodes
    auto node = std::min_element(nodes.begin(), nodes.end(),
                                 [cartPos](const FGTaxiNodeRef& a, const FGTaxiNodeRef& b) { return distSqr(cartPos, a->cart()) < distSqr(cartPos, b->cart()); });

    if (node == nodes.end()) {
        return FGTaxiNodeRef();
    }

    return *node;
}

/**
 * Returns the nearest node on the runway where the connected segment is in direction of runway heading. Falls back to ones behind aircraft
 * @param aGeod Reference point
 * @param aRunway the runway to consider
 * @return
 *
 */

FGTaxiNodeRef FGGroundNetwork::findNearestNodeOnRunwayEntry(const SGGeod& aGeod, const FGRunway* aRunway) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result;
    FGTaxiNodeVector::const_iterator it;
    if (!aRunway) {
        SG_LOG(SG_AI, SG_DEV_WARN, "No Runway provided");
        return nullptr;
    }
    SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry " << aRunway->ident() << " " << aRunway->headingDeg());
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        double headingTowardsEntry = SGGeodesy::courseDeg(aGeod, (*it)->geod());
        double diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - headingTowardsEntry));
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry Diff : " << diff << " Id : " << (*it)->getIndex());
        if (diff > 10) {
            // Only down the runway not backwards
            continue;
        }
        FGTaxiSegmentVector entrySegments = findSegmentsFrom((*it));
        // Some kind of star
        if (entrySegments.size() > 2) {
            continue;
        }
        // two segments and next points are on runway, too. Must be a segment before end
        // single runway point not at end is ok
        if (entrySegments.size() == 2 &&
            ((*entrySegments.at(0)->getEnd()).getIsOnRunway() ||
             (*entrySegments.at(1)->getEnd()).getIsOnRunway())) {
            continue;
        }
        if (entrySegments.empty()) {
            SG_LOG(SG_AI, SG_ALERT, "findNearestNodeOnRunwayEntry broken node :" << diff << " Node Id : " << (*it)->getIndex() << " Apt : " << aRunway->airport()->getId());
            continue;
        }
        double entryHeading = SGGeodesy::courseDeg((entrySegments.back())->getEnd()->geod(),
                                                   (*it)->geod());
        diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - entryHeading));
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry2 Diff :" << diff << " Rwy Heading " << aRunway->headingDeg() << " Entry " << entryHeading << " Id : " << (*it)->getIndex() << " " << aRunway->ident());
        if (diff > 75) {
            // Only entries going in our direction
            continue;
        }
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry3 " << localDistanceSqr << " " << (*it)->getIndex());
            d = localDistanceSqr;
            result = *it;
        }
    }
    if (result) {
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry found :" << result->getIndex());
        return result;
    }
    // Ok then fallback to old algorithm ignoring direction
    d = DBL_MAX;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayEntry from Threshold " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }

    return result;
}

/**
 * Returns the nearest node in that is in direction of runway heading. Falls back to ones behind aircraft
 * @param aGeod Reference point
 * @param aRunway the runway to consider
 * @return
 *
 */
FGTaxiNodeRef FGGroundNetwork::findNearestNodeOnRunwayExit(const SGGeod& aGeod, const FGRunway* aRunway) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result = 0;
    FGTaxiNodeVector::const_iterator it;
    if (aRunway) {
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit " << aRunway->ident() << " " << aRunway->headingDeg());
        for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (!(*it)->getIsOnRunway())
                continue;
            double localDistanceSqr = distSqr(cartPos, (*it)->cart());
            double headingTowardsExit = SGGeodesy::courseDeg(aGeod, (*it)->geod());
            double diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - headingTowardsExit));
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit Diff : " << diff << " Id : " << (*it)->getIndex());
            if (diff > 10) {
                // Only ahead
                continue;
            }
            FGTaxiSegmentVector exitSegments = findSegmentsFrom((*it));
            // Some kind of star
            if (exitSegments.size() > 2) {
                continue;
            }
            // two segments and next points are on runway, too. Must be a segment before end
            // single runway point not at end is ok
            if (exitSegments.size() == 2 &&
                ((*exitSegments.at(0)->getEnd()).getIsOnRunway() ||
                 (*exitSegments.at(1)->getEnd()).getIsOnRunway())) {
                continue;
            }
            if (exitSegments.empty()) {
                SG_LOG(SG_AI, SG_ALERT, "findNearestNodeOnRunwayExit broken node :" << diff << " Node Id : " << (*it)->getIndex() << " Apt : " << aRunway->airport()->getId());
                continue;
            }
            double exitHeading = SGGeodesy::courseDeg((*it)->geod(),
                                                      (exitSegments.back())->getEnd()->geod());
            diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - exitHeading));
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit2 Diff :" << diff << " Id : " << (*it)->getIndex());
            if (diff > 70) {
                // Only exits going in our direction
                continue;
            }
            if (localDistanceSqr < d) {
                SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit3 " << localDistanceSqr << " " << (*it)->getIndex());
                d = localDistanceSqr;
                result = *it;
            }
        }
    } else {
        SG_LOG(SG_AI, SG_ALERT, "No Runway findNearestNodeOnRunwayExit");
    }
    if (result) {
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit found :" << result->getIndex());
        return result;
    }
    // Ok then fallback only exits ahead
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (aRunway) {
            double headingTowardsExit = SGGeodesy::courseDeg(aGeod, (*it)->geod());
            double diff = fabs(aRunway->headingDeg() - headingTowardsExit);
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback1 " << aRunway->headingDeg() << " " << " Diff : " << diff << " " << (*it)->getIndex());
            if (diff > 10) {
                // Only ahead
                continue;
            }
        }
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback1 " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }
    if (result) {
        return result;
    }
    // Ok then fallback only exits
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback2 " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }
    if (result) {
        return result;
    } else {
        SG_LOG(SG_AI, SG_WARN, "No runway exit found " << aRunway->airport()->getId() << "/" << aRunway->name());
        return 0;
    }
}

FGTaxiSegment* FGGroundNetwork::findOppositeSegment(unsigned int index) const
{
    FGTaxiSegment* seg = findSegment(index);
    if (!seg)
        return NULL;
    return seg->opposite();
}

FGIntersectedTaxiSegment* FGGroundNetwork::findIntersectionSegment(const SGGeod& start, double heading) const
{
    // Create a line segment from point in direction of heading
    auto end = SGGeodesy::direct(start, heading, 500.0);

    double dist = DBL_MAX;
    FGIntersectedTaxiSegment* ret = nullptr;
    for (auto seg : segments) {
        auto intersection = SGGeodesy::intersection(start, end, seg->getStart()->geod(), seg->getEnd()->geod());
        if (intersection.has_value()) {
            const double newDist = SGGeodesy::distanceM(start, (*intersection));
            const double dist1 = SGGeodesy::distanceM(seg->getStart()->geod(), (*intersection));
            const double dist2 = SGGeodesy::distanceM(seg->getEnd()->geod(), (*intersection));
            const double segmentLen = seg->getLength();

            const double headingIntersection = SGMiscd::round(SGGeodesy::courseDeg(start, (*intersection)));

            if (newDist > 0 && newDist < dist && SGMiscd::round(heading) == headingIntersection && dist1 < segmentLen && dist2 < segmentLen) {
                dist = newDist;
                ret = new FGIntersectedTaxiSegment(seg->getStart(), (*intersection), seg->getEnd());
            }
        }
    }

    SG_LOG(SG_AI, SG_BULK, "No intersection segment found at " << parent->getId());
    return ret;
}

const FGParkingList& FGGroundNetwork::allParkings() const
{
    return m_parkings;
}

FGTaxiSegment* FGGroundNetwork::findSegment(unsigned idx) const
{
    if ((idx > 0) && (idx <= segments.size()))
        return segments[idx - 1];
    else {
        //cerr << "Alert: trying to find invalid segment " << idx << endl;
        return 0;
    }
}

FGTaxiSegment* FGGroundNetwork::findSegment(const FGTaxiNode* from, const FGTaxiNode* to) const
{
    if (from == 0) {
        return NULL;
    }

    // completely boring linear search of segments. Can be improved if/when
    // this ever becomes a hot-spot
    for (auto seg : segments) {
        if (seg->startNode != from) {
            continue;
        }

        if ((to == 0) || (seg->endNode == to)) {
            return seg;
        }
    }

    return NULL; // not found
}

/**
 * Calculate a penalty for an TaxiSegment
 *
 */

static int edgePenalty(FGTaxiSegment* ts)
{
    FGTaxiNode* tn = ts->getEnd();
    int penalty = (tn->type() == FGPositioned::PARKING ? 10000 : 0) +
                  (tn->getHoldPointType() == 1 ? 1000 : 0) +
                  (tn->getHoldPointType() == 2 ? 1000 : 0) +
                  (tn->getIsOnRunway() ? 1000 : 0);
    // Add the precalculated penalty
    penalty += ts->getPenalty();
    return penalty;
}

class ShortestPathData
{
public:
    ShortestPathData() : score(HUGE_VAL)
    {
    }

    double distance;
    double score;
    FGTaxiNodeRef previousNode;
};

FGTaxiRoute FGGroundNetwork::findShortestRoute(FGTaxiNode* start, FGTaxiNode* end, bool fullSearch) const
{
    if (!start) {
        throw sg_exception("Bad arguments to findShortestRoute: start node is null");
    }
    if (!end) {
        throw sg_exception("Bad arguments to findShortestRoute: end node is null");
    }
    //implements Dijkstra's algorithm to find shortest distance route from start to end
    //taken from http://en.wikipedia.org/wiki/Dijkstra's_algorithm
    FGTaxiNodeVector unvisited(m_nodes);
    std::map<FGTaxiNode*, ShortestPathData> searchData;

    searchData[start].distance = 0.0;
    searchData[start].score = 0.0;

    while (!unvisited.empty()) {
        // find lowest scored unvisited
        FGTaxiNodeRef best = unvisited.front();
        for (auto i : unvisited) {
            if (searchData[i].score < searchData[best].score) {
                best = i;
            }
        }

        // remove 'best' from the unvisited set
        FGTaxiNodeVectorIterator newend =
            remove(unvisited.begin(), unvisited.end(), best);
        unvisited.erase(newend, unvisited.end());

        if (best == end) { // found route or best not connected
            break;
        }

        for (auto target : findSegmentsFrom(best)) {
            double edgeLength = dist(best->cart(), target->getEnd()->cart());
            double alt = searchData[best].score + edgeLength + edgePenalty(target);
            if (alt < searchData[target->getEnd()].score) { // Relax (u,v)
                searchData[target->getEnd()].distance = searchData[best].distance + edgeLength;
                searchData[target->getEnd()].score = alt;
                searchData[target->getEnd()].previousNode = best;
            }
        } // of outgoing arcs/segments from current best node iteration
    } // of unvisited nodes remaining

    if (searchData[end].score == HUGE_VAL) {
        // no valid route found
        if (fullSearch && start && end) {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "Failed to find route from waypoint " << start->getIndex() << " to "
                                                         << end->getIndex() << " at " << parent->getId());
        }

        return FGTaxiRoute();
    }

    // assemble route from backtrace information
    FGTaxiNodeVector nodes;
    intVec routes;
    FGTaxiNode* bt = end;

    while (searchData[bt].previousNode != 0) {
        nodes.push_back(bt);
        FGTaxiSegment* segment = findSegment(searchData[bt].previousNode, bt);
        int idx = segment->getIndex();
        routes.push_back(idx);
        bt = searchData[bt].previousNode;
    }
    nodes.push_back(start);
    reverse(nodes.begin(), nodes.end());
    reverse(routes.begin(), routes.end());
    return FGTaxiRoute(nodes, routes, searchData[end].distance, searchData[end].score, 0);
}

void FGGroundNetwork::unblockAllSegments(time_t now)
{
    for (auto& seg : segments) {
        seg->unblock(now);
    }
}

void FGGroundNetwork::blockSegmentsEndingAt(const FGTaxiSegment* seg, int blockId, time_t blockTime, time_t now)
{
    if (!seg) {
        throw sg_exception("Passed invalid segment");
    }

    long i = 0;
    const auto range = m_segmentsEndingAtNodeMap.equal_range(seg->getEnd());
    for (auto it = range.first; it != range.second; ++it) {
        // our inbound segment will be included, so skip it
        if (it->second == seg)
            continue;

        i++;
        it->second->block(blockId, blockTime, now);
    }
    SG_LOG(SG_ATC, SG_BULK, "blockSegmentsEndingAt " << "\t" << i << "\t" << seg->getIndex() << "\t" << blockId);
}

FGTaxiNodeRef FGGroundNetwork::findNodeByIndex(int index) const
{
    FGTaxiNodeVector::const_iterator it;
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if ((*it)->getIndex() == index) {
            return *it;
        }
    }

    return FGTaxiNodeRef();
}

FGParkingRef FGGroundNetwork::getParkingByIndex(unsigned int index) const
{
    FGTaxiNodeRef tn = findNodeByIndex(index);
    if (!tn.valid() || (tn->type() != FGPositioned::PARKING)) {
        return FGParkingRef();
    }

    return FGParkingRef(static_cast<FGParking*>(tn.ptr()));
}

FGParkingRef FGGroundNetwork::findParkingByName(const string& name) const
{
    auto it = std::find_if(m_parkings.begin(), m_parkings.end(), [name](const FGParkingRef& p) {
        return p->ident() == name;
    });

    if (it == m_parkings.end())
        return nullptr;

    return *it;
}

void FGGroundNetwork::addSegment(const FGTaxiNodeRef& from, const FGTaxiNodeRef& to)
{
    FGTaxiSegment* seg = new FGTaxiSegment(from, to);

    segments.push_back(seg);

    FGTaxiNodeVector::iterator it = std::find(m_nodes.begin(), m_nodes.end(), from);
    if (it == m_nodes.end()) {
        m_nodes.push_back(from);
    }

    it = std::find(m_nodes.begin(), m_nodes.end(), to);
    if (it == m_nodes.end()) {
        m_nodes.push_back(to);
    }
}

void FGGroundNetwork::addParking(const FGParkingRef& park)
{
    m_parkings.push_back(park);


    FGTaxiNodeVector::iterator it = std::find(m_nodes.begin(), m_nodes.end(), park);
    if (it == m_nodes.end()) {
        m_nodes.push_back(park);
    }
}

FGTaxiSegmentVector FGGroundNetwork::findSegmentsFrom(const FGTaxiNodeRef& from) const
{
    FGTaxiSegmentVector result;
    FGTaxiSegmentVector::const_iterator it;
    for (it = segments.begin(); it != segments.end(); ++it) {
        if ((*it)->getStart() == from) {
            result.push_back((*it));
        }
    }

    return result;
}

FGTaxiSegment* FGGroundNetwork::findSegmentByHeading(const FGTaxiNode* from, const double heading) const
{
    if (from == 0) {
        return NULL;
    }

    FGTaxiSegment* best = nullptr;

    // completely boring linear search of segments. Can be improved if/when
    // this ever becomes a hot-spot
    for (auto seg : segments) {
        if (seg->startNode != from) {
            continue;
        }

        if (!best || fabs(best->getHeading() - heading) > fabs(seg->getHeading() - heading)) {
            best = seg;
        }
    }

    return best; // not found
}

const intVec& FGGroundNetwork::getApproachFrequencies() const
{
    return freqApproach;
}

const intVec& FGGroundNetwork::getTowerFrequencies() const
{
    return freqTower;
}

const intVec& FGGroundNetwork::getGroundFrequencies() const
{
    return freqGround;
}
