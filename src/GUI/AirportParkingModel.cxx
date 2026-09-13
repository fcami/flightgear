// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner

#include "config.h"

#include "AirportParkingModel.hxx"

#include <algorithm>

#include <Airports/airport.hxx>
#include <Airports/groundnetwork.hxx>
#include <Airports/parking.hxx>

using namespace std::string_literals;

AirportParkingModel::AirportParkingModel(const std::string& icao)
{
    setIcao(icao);
}

AirportParkingModel::AirportParkingModel(const FGAirportRef& airport)
{
    setAirport(airport);
}

void AirportParkingModel::setIcao(const std::string& icao)
{
    setAirport(FGAirport::findByIdent(icao));
}

void AirportParkingModel::setAirport(const FGAirportRef& airport)
{
    if (airport == _airport) {
        return;
    }

    _airport = airport;
    requery();
}

void AirportParkingModel::setParkingType(const std::string& type)
{
    if (type == _parkingType) {
        return;
    }

    _parkingType = type;
    requery();
}

void AirportParkingModel::requery()
{
    _parkings.clear();

    FGGroundNetwork* net = _airport ? _airport->groundNetwork() : nullptr;
    if (!net) {
        didReset();
        return;
    }

    const auto& allParkings = net->allParkings();
    if (_parkingType.empty()) {
        _parkings = allParkings;
    } else {
        std::copy_if(allParkings.begin(), allParkings.end(), std::back_inserter(_parkings),
                     [this](const FGParkingRef& p) { return p->getType() == _parkingType; });
    }

    didReset();
}

size_t AirportParkingModel::count() const
{
    return _parkings.size();
}

std::any AirportParkingModel::dataAt(size_t index, const std::string& key) const
{
    if (index >= _parkings.size()) {
        return {};
    }

    const auto& p = _parkings.at(index);
    if (key == "label") {
        return p->getName();
    } else if (key == "value") {
        return p->getName();
    } else if (key == "name") {
        return p->getName();
    } else if (key == "type") {
        return p->getType();
    } else if (key == "airlineCodes") {
        return p->getCodes();
    } else if (key == "heading") {
        return p->getHeading();
    } else if (key == "radius") {
        return p->getRadius();
    }

    return {};
}
