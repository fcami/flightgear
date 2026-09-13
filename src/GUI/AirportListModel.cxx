// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner

#include "config.h"

#include "AirportListModel.hxx"

#include <algorithm>

#include "Navaids/NavDataCache.hxx"
#include <Airports/airport.hxx>
#include <Main/globals.hxx>

using namespace std::string_literals;

AirportListModel::AirportListModel() = default;

void AirportListModel::setSearchTerm(const std::string& search)
{
    if (search == _searchTerm) {
        return;
    }

    _searchTerm = search;
    _nearbyRangeNm = 0.0; // clear this
    requery();
}


void AirportListModel::setQueryHeliports(bool b)
{
    _useHeliports = b;
    requery();
}

void AirportListModel::setNearbyRangeNm(double rangeNm)
{
    if (rangeNm == _nearbyRangeNm) {
        return;
    }

    _nearbyRangeNm = rangeNm;
    requery();
}

void AirportListModel::requery()
{
    _data.clear();

    if (_nearbyRangeNm > 0.0) {
        useNearbyAirports();
        return;
    }

    if (_searchTerm.empty()) {
        useRecentAirports();
        return;
    }

    if (_searchTerm.size() < 3) {
        SG_LOG(SG_GUI, SG_INFO, "AirportListModel::requery: search term too short");
        didReset();
        return;
    }

    const auto typeMin = _useHeliports ? FGPositioned::HELIPORT : FGPositioned::AIRPORT;
    const auto typeMax = _useHeliports ? FGPositioned::HELIPORT : FGPositioned::SEAPORT;
    flightgear::NavDataCache::instance()->searchAirports(_searchTerm, _data, typeMin, typeMax);
    didReset();
}

void AirportListModel::addRecentEntry(const FGAirportRef apt)
{
    // Deduplicate: remove any existing entry for this airport before appending.
    _recents.erase(std::remove(_recents.begin(), _recents.end(), apt), _recents.end());
    _recents.push_back(apt);

    // Cap the recents list to avoid unbounded growth.
    constexpr size_t kMaxRecents = 128;
    if (_recents.size() > kMaxRecents) {
        _recents.erase(_recents.begin());
    }
}

void AirportListModel::useRecentAirports()
{
    _data.clear();
    for (auto& apt : _recents) {
        _data.emplace_back(apt->name(), apt->ident(), apt->guid());
    }
    didReset();
}

void AirportListModel::useNearbyAirports()
{
    _data.clear();

    FGAirport::TypeRunwayFilter filter; // defaults to airports only
    if (_useHeliports) {
        filter.fromTypeString("heliport"s);
    }

    const SGGeod pos = globals->get_aircraft_position();
    FGPositionedList apts = FGPositioned::findWithinRange(pos, _nearbyRangeNm, &filter);
    FGPositioned::sortByRange(apts, pos);

    for (const auto& a : apts) {
        if (const auto apt = fgpositioned_cast<FGAirport>(a)) {
            _data.emplace_back(apt->name(), apt->ident(), apt->guid());
        }
    }

    didReset();
}

size_t AirportListModel::count() const
{
    return _data.size();
}

std::any AirportListModel::dataAt(size_t index, const std::string& key) const
{
    if (index >= _data.size()) {
        return {};
    }

    const auto& d = _data.at(index);
    if (key == "label") {
        return d.icao + " "s + d.name;
    } else if (key == "value") {
        return d.id;
    } else if (key == "icao") {
        return d.icao;
    }

    return {};
}
