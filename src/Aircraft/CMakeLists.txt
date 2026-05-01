# SPDX-FileCopyrightText: 2010 James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later


set(SOURCES
	controls.cxx
	replay.cxx
	flightrecorder.cxx
	FlightHistory.cxx
	initialstate.cxx
	AircraftPerformance.cxx
	replay-internal.cxx
	continuous.cxx
	)

set(HEADERS
	controls.hxx
	replay.hxx
	flightrecorder.hxx
	FlightHistory.hxx
	initialstate.hxx
	AircraftPerformance.hxx
	continuous.hxx
	replay-internal.hxx
	)


target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
