# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2011 James Turner <james@flightgear.org>


target_sources(fgfsObjects PRIVATE
	audioident.cxx
	soundgenerator.cxx
	beacon.cxx
	fg_fx.cxx
	morse.cxx
	sample_queue.cxx
	voice.cxx
	voiceplayer.cxx
	soundmanager.cxx
	audioident.hxx
	soundgenerator.hxx
	beacon.hxx
	fg_fx.hxx
	morse.hxx
	sample_queue.hxx
	voice.hxx
	voiceplayer.hxx
	soundmanager.hxx
	VoiceSynthesizer.hxx
	flitevoice.hxx
)

add_library(fgvoicesynth STATIC VoiceSynthesizer.cxx flitevoice.cxx)
target_link_libraries(fgvoicesynth PRIVATE flightgear_flite_hts SimGearScene)
