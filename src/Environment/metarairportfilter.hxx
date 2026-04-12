// metarairportfilter.hxx -- Implementation of AirportFilter
//
// Written by Torsten Dreyer, August 2010
//
// SPDX-FileCopyrightText: 2010 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Airports/airport.hxx>
#include <simgear/props/props.hxx>

namespace Environment {

/**
 * @brief A AirportFilter for selection airports that provide a METAR
 * Singleton implementation of FGAirport::AirportFilter
 */
class MetarAirportFilter : public FGAirport::AirportFilter {
public:
    static MetarAirportFilter * instance();
protected:
    MetarAirportFilter() {}
    virtual bool passAirport(FGAirport* aApt) const {
        return aApt->hasMetar();
    }

  // permit heliports and seaports too
  virtual FGPositioned::Type maxType() const
  { return FGPositioned::SEAPORT; }
private:
  static MetarAirportFilter * _instance;
};

} // namespace Environment
