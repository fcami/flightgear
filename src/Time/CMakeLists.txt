# SPDX-FileCopyrightText: 2010 James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later


target_sources(fgfsObjects PRIVATE
	TimeManager.cxx
	light.cxx
        tide.cxx
	bodysolver.cxx
	TimeManager.hxx
	light.hxx
        tide.hxx
	bodysolver.hxx
)
