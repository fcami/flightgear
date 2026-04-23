# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later


set(SOURCES
	antenna.cxx
	radio.cxx
	)

set(HEADERS
	antenna.hxx
	radio.hxx
	)


target_sources(fgfsObjects PRIVATE ${SOURCES} ${HEADERS})
