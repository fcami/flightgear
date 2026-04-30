# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

set(SOURCES
	SceneryPager.cxx
	scenery.cxx
	terrain_stg.cxx
	terrain_pgt.cxx
	tilecache.cxx
	tileentry.cxx
	tilemgr.cxx
	marker.cxx
	)

set(HEADERS
	SceneryPager.hxx
	scenery.hxx
	terrain.hxx
	terrain_stg.hxx
	terrain_pgt.hxx
	tilecache.hxx
	tileentry.hxx
	tilemgr.hxx
	marker.hxx
	)

target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
