# SPDX-FileCopyrightText: 2010 James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later


set(SOURCES
	electrical.cxx
	pitot.cxx
	static.cxx
	system_mgr.cxx
	vacuum.cxx
	)

set(HEADERS
	electrical.hxx
	pitot.hxx
	static.hxx
	system_mgr.hxx
	vacuum.hxx
	)


target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
