// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 James Turner

#pragma once

#include <Airports/airports_fwd.hxx>
#include <Airports/parking.hxx>
#include <GUI/ItemModel.hxx>

/**
 * @brief Exposes the FGParking positions of a single airport as an ItemModel.
 */
class AirportParkingModel : public ItemModel
{
public:
    AirportParkingModel() = default;
    explicit AirportParkingModel(const std::string& icao);
    explicit AirportParkingModel(const FGAirportRef& airport);

    FGAirportRef airport() const { return _airport; }

    /**
     * @brief Set the airport to list parkings for, by ICAO ident. Passing an
     * unknown ident clears the model (an empty list).
     */
    void setIcao(const std::string& icao);

    /**
     * @brief Set the airport to list parkings for, directly.
     */
    void setAirport(const FGAirportRef& airport);

    std::string parkingType() const { return _parkingType; }

    /**
     * @brief Filter the listed parkings by type (eg "gate", "tie-down",
     * "cargo"). Pass an empty string to disable the filter and list all
     * parkings.
     */
    void setParkingType(const std::string& type);

    size_t count() const override;
    std::any dataAt(size_t index, const std::string& key) const override;

private:
    void requery();

    FGAirportRef _airport;
    std::string _parkingType;
    FGParkingList _parkings;
};
