/**
 * Polyline - store geographic line-segments */

// Written by James Turner, started 2013.
//
// SPDX-FileCopyrightText: 2013 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <simgear/sg_inlines.h>
#include <simgear/structure/SGSharedPtr.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/math/SGBox.hxx>
#include <simgear/math/SGGeometryFwd.hxx>

namespace flightgear
{

typedef std::vector<SGGeod> SGGeodVec;

class PolyLine;

typedef SGSharedPtr<PolyLine> PolyLineRef;
typedef std::vector<PolyLineRef> PolyLineList;

/**
 * @class Store geographical linear data, with a type code.
 *
 * This is a basic in-memory model of GIS line data, without support for
 * many features; especially there is no support for per-node attributes.
 *
 * PolyLines are added to the spatial index and can be queried by passing
 * a search centre and cutoff distance.
 */
class PolyLine : public SGReferenced
{
public:
    virtual ~PolyLine();

    enum Type {
        INVALID = 0,
        COASTLINE,
        NATIONAL_BOUNDARY, /// aka a border
        REGIONAL_BOUNDARY, /// state / province / country / department
        RIVER,
        LAKE,
        URBAN,
        LAND_MASS,
        GRATICULE,
        GEOGRAPHIC_LINE,
        // airspace types in the future
        LAST_TYPE
    };

    Type type() const
    { return m_type; }

    /**
     * number of points in this line - at least two.
     */
    unsigned int numPoints() const;

    SGGeod point(unsigned int aIndex) const;

    const SGGeodVec& points() const
    { return m_data; }

    /**
     * create poly line objects from raw input points and a type.
     * input points will be subdivided so the bounding area of each
     * polyline stays within some threshold.
     *
     */
    static PolyLineList createChunked(Type aTy, const SGGeodVec& aRawPoints);

    static PolyLineRef create(Type aTy, const SGGeodVec& aRawPoints);

    static void bulkAddToSpatialIndex(PolyLineList::const_iterator begin,
                                      PolyLineList::const_iterator end);

    /**
     * retrieve all the lines within a range of a search point.
     * lines are returned if any point is near the search location.
     */
    static PolyLineList linesNearPos(const SGGeod& aPos, double aRangeNm, Type aTy);

    class TypeFilter
    {
    public:
        virtual bool pass(Type aTy) const = 0;
    };

    static PolyLineList linesNearPos(const SGGeod& aPos, double aRangeNm, const TypeFilter& aFilter);

    const SGBoxd& cartesianBox() const;

    void addToSpatialIndex() const;

private:
    PolyLine(Type aTy, const SGGeodVec& aPointVec);

    Type m_type;
    SGGeodVec m_data;
    SGBoxd m_box;
};


} // namespace flightgear
