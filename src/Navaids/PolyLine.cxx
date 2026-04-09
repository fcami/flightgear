/**
 * Polyline - store geographic line-segments */

// Written by James Turner, started 2013.
//
// SPDX-FileCopyrightText: 2013 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "PolyLine.hxx"

#include <cassert>

#include <simgear/math/sg_geodesy.hxx>
#include <simgear/structure/exception.hxx>

#include <Navaids/PositionedOctree.hxx>

using namespace flightgear;

PolyLine::PolyLine(Type aTy, const SGGeodVec& points) : m_type(aTy),
                                                        m_data(points)
{
    assert(!points.empty());
    for (const auto& pt : m_data) {
        m_box.expandBy(SGVec3d::fromGeod(pt));
    }
}

PolyLine::~PolyLine() = default;

unsigned int PolyLine::numPoints() const
{
    return m_data.size();
}

SGGeod PolyLine::point(unsigned int aIndex) const
{
    assert(aIndex <= m_data.size());
    return m_data[aIndex];
}

PolyLineList PolyLine::createChunked(Type aTy, const SGGeodVec& aRawPoints)
{
    PolyLineList result;
    if (aRawPoints.size() < 2) {
        return result;
    }

    const double maxDistanceSquaredM = 40000 * 40000; // 40km to start with

    SGVec3d chunkStartCart = SGVec3d::fromGeod(aRawPoints.front());
    SGGeodVec chunk;
    SGGeodVec::const_iterator it = aRawPoints.begin();

    while (it != aRawPoints.end()) {
        SGVec3d ptCart = SGVec3d::fromGeod(*it);
        double d2 = distSqr(chunkStartCart, ptCart);

    // distance check, but also ensure we generate actual valid line segments.
        if ((chunk.size() >= 2) && (d2 > maxDistanceSquaredM)) {
            chunk.push_back(*it); // close the segment
            result.push_back(new PolyLine(aTy, chunk));
            chunkStartCart = ptCart;
            chunk.clear();
        }

        chunk.push_back(*it++); // add to open chunk
    }

    // if we have a single trailing point, we already added it as the last
    // point of the previous chunk, so we're ok. Otherwise, create the
    // final chunk's polyline
    if (chunk.size() > 1) {
        result.push_back(new PolyLine(aTy, chunk));
    }

    return result;
}

PolyLineRef PolyLine::create(PolyLine::Type aTy, const SGGeodVec &aRawPoints)
{
    return new PolyLine(aTy, aRawPoints);
}

void PolyLine::bulkAddToSpatialIndex(PolyLineList::const_iterator begin,
                                     PolyLineList::const_iterator end)
{
    // Phase 1: collect (box, line) pairs - boxes are already cached, no compute needed
    std::vector<std::pair<SGBoxd, PolyLineRef>> entries;
    entries.reserve(std::distance(begin, end));
    for (auto it = begin; it != end; ++it) {
        entries.emplace_back((*it)->cartesianBox(), *it);
    }

    // Phase 2: insert into octree serially
    Octree::Node* root = Octree::globalTransientOctree();
    for (const auto& [box, line] : entries) {
        root->findNodeForBox(box)->addPolyLine(const_cast<PolyLine*>(line.get()));
    }
}

void PolyLine::addToSpatialIndex() const
{
    Octree::Node* node = Octree::globalTransientOctree()->findNodeForBox(cartesianBox());
    node->addPolyLine(const_cast<PolyLine*>(this));
}

const SGBoxd& PolyLine::cartesianBox() const
{
    return m_box;
}

class SingleTypeFilter : public PolyLine::TypeFilter
{
public:
    SingleTypeFilter(PolyLine::Type aTy) :
        m_type(aTy)
    { }

    virtual bool pass(PolyLine::Type aTy) const
    { return (aTy == m_type); }
private:
    PolyLine::Type m_type;
};

PolyLineList PolyLine::linesNearPos(const SGGeod& aPos, double aRangeNm, Type aTy)
{
    return linesNearPos(aPos, aRangeNm, SingleTypeFilter(aTy));
}


PolyLineList PolyLine::linesNearPos(const SGGeod& aPos, double aRangeNm, const TypeFilter& aFilter)
{
    std::set<PolyLineRef> resultSet;

    SGVec3d cart = SGVec3d::fromGeod(aPos);
    double cutoffM = aRangeNm * SG_NM_TO_METER;
    Octree::FindLinesDeque deque;
    deque.push_back(Octree::globalTransientOctree());

    while (!deque.empty()) {
        Octree::Node* n = deque.front();
        deque.pop_front();

        PolyLineList lines;
        n->visitForLines(cart, cutoffM, lines, deque);

        // merge into result set, filtering as we go.
        for (auto ref : lines) {
            if (aFilter.pass(ref->type())) {
                resultSet.insert(ref);
            }
        }
    } // of deque iteration

    PolyLineList result;
    result.insert(result.end(), resultSet.begin(), resultSet.end());
    return result;
}
