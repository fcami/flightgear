# SPDX-FileCopyrightText: 2010 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfsObjects PRIVATE
        AirportGroundRadar.cxx
	atc_mgr.cxx
	trafficcontrol.cxx
	CommStation.cxx
        ATISEncoder.cxx
        MetarPropertiesATISInformationProvider.cxx
        CurrentWeatherATISInformationProvider.cxx
        ATCController.cxx
        ApproachController.cxx
        EnRouteController.cxx
        GroundController.cxx
        StartupController.cxx
        TowerController.cxx
        AirportGroundRadar.hxx
	atc_mgr.hxx
	trafficcontrol.hxx
	CommStation.hxx
        ATISEncoder.hxx
        MetarPropertiesATISInformationProvider.hxx
        CurrentWeatherATISInformationProvider.hxx
        ATCController.hxx
        ApproachController.hxx
        EnRouteController.hxx
        GroundController.hxx
        StartupController.hxx
        TowerController.hxx
        QuadTree.hxx
)
