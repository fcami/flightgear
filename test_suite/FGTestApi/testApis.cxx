// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

// implement various test-only methods on classes

#include <Airports/groundnetwork.hxx>
#include <Airports/airport.hxx>
#include <Airports/xmlloader.hxx>

void FGAirport::testSuiteInjectGroundnetXML(const SGPath& path)
{
    _groundNetwork.reset(new FGGroundNetwork(const_cast<FGAirport*>(this)));
    XMLLoader::loadFromPath(_groundNetwork.get(), path);
    _groundNetwork->init();
}
