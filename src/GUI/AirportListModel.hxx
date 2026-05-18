// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner

#pragma once

#include "Navaids/positioned.hxx"

#include <Airports/airports_fwd.hxx>
#include <GUI/ItemModel.hxx>
#include <Navaids/NavDataCache.hxx>

class AirportListModel : public ItemModel
{
public:
    AirportListModel();

    std::string searchTerm() const
    {
        return _searchTerm;
    }

    /**
     * @brief Set the search term: an ICAO code or part of the airport's full name
     *
     */
    void setSearchTerm(const std::string& search);

    // set airport type (heliport / seaport / etc)
    //
    void setQueryHeliports(bool b);

    void addRecentEntry(const FGAirportRef apt);

    size_t count() const override;
    std::any dataAt(size_t index, const std::string& key) const override;

private:
    void requery();
    void useRecentAirports();

    std::string _searchTerm;
    bool _useHeliports = false;
    FGPositionedList _recents;

    std::vector<flightgear::NavDataCache::AirportDesc> _data;
};
