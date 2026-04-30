// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner

#include "config.h"

#include "AirportListModel.hxx"

#include <algorithm>

#include "Navaids/NavDataCache.hxx"
#include <Airports/airport.hxx>

using namespace std::string_literals;

AirportListModel::AirportListModel() = default;

void AirportListModel::setSearchTerm(const std::string& search)
{
    if (search == _searchTerm) {
        return;
    }

    _searchTerm = search;
    requery();
}


void AirportListModel::setQueryHeliports(bool b)
{
    _useHeliports = b;
    requery();
}

void AirportListModel::requery()
{
    _data.clear();

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
