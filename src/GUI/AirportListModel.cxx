// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner

#include "config.h"

#include "AirportListModel.hxx"

#include "Navaids/NavDataCache.hxx"

using namespace std::string_literals;

AirportListModel::AirportListModel() = default;

void AirportListModel::setSearchTerm(const std::string &search)
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
        return;
    }

    flightgear::NavDataCache::instance()->searchAirports(_searchTerm, _data);
    didReset();
}

void AirportListModel::addRecentEntry(const FGAirportRef apt)
{
    _recents.insert(_recents.end(), apt);
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
