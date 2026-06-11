# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later


target_sources(fgfsObjects PRIVATE
	atmosphere.cxx
	atmospheric_profile.cxx
	environment.cxx
	environment_ctrl.cxx
	environment_mgr.cxx
	ephemeris.cxx
    climate.cxx
	fgclouds.cxx
	fgmetar.cxx
	metarairportfilter.cxx
	metarproperties.cxx
	openmeteo_data.cxx
	openmeteo_manager.cxx
	openmeteo_request.cxx
	precipitation_mgr.cxx
	realwx_ctrl.cxx
	ridge_lift.cxx
	terrainsampler.cxx
	presets.cxx
	gravity.cxx
    magvarmanager.cxx
	atmosphere.hxx
	atmospheric_profile.hxx
	environment.hxx
	environment_ctrl.hxx
	environment_mgr.hxx
	ephemeris.hxx
	fgclouds.hxx
	climate.hxx
	fgmetar.hxx
	metarairportfilter.hxx
	metarproperties.hxx
	openmeteo_data.hxx
	openmeteo_manager.hxx
	openmeteo_request.hxx
	precipitation_mgr.hxx
	realwx_ctrl.hxx
	ridge_lift.hxx
	terrainsampler.hxx
	presets.hxx
	gravity.hxx
    magvarmanager.hxx
	weather_property_writer.cxx
	weather_property_writer.hxx
)
