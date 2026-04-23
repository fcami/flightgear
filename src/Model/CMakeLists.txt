# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2011 James Turner <james@flightgear.org>


set(SOURCES
	acmodel.cxx
	modelmgr.cxx
	validateSharedModels.cxx
	)

set(HEADERS
	acmodel.hxx
	modelmgr.hxx
	validateSharedModels.hxx
	)

target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
