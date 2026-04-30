# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later


target_sources(fgfsObjects PRIVATE
	airways.cxx
	fixlist.cxx
	markerbeacon.cxx
	navdb.cxx
	navlist.cxx
	navrecord.cxx
	poidb.cxx
	positioned.cxx
	procedure.cxx
	route.cxx
	routePath.cxx
	waypoint.cxx
    LevelDXML.cxx
    FlightPlan.cxx
    NavDataCache.cxx
    PositionedOctree.cxx
    PolyLine.cxx
    SHPParser.cxx
    airways.hxx
	fixlist.hxx
	markerbeacon.hxx
	navdb.hxx
	navlist.hxx
	navrecord.hxx
	poidb.hxx
	positioned.hxx
	procedure.hxx
	route.hxx
	routePath.hxx
	waypoint.hxx
    LevelDXML.hxx
    FlightPlan.hxx
    NavDataCache.hxx
    PositionedOctree.hxx
    PolyLine.hxx
    SHPParser.hxx
    CacheSchema.h
)
