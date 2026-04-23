# SPDX-FileCopyrightText: 2010 James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later


set(SOURCES
	TimeManager.cxx
	light.cxx
        tide.cxx
	bodysolver.cxx
	)

set(HEADERS
	TimeManager.hxx
	light.hxx
        tide.hxx
	bodysolver.hxx
	)

target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
