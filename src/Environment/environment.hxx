// environment.hxx -- routines to model the natural environment.
//
// Written by David Megginson, started February 2002.
//
// SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/compiler.h>

#include <cmath>
#include <simgear/math/SGGeod.hxx>
#include <simgear/props/tiedpropertylist.hxx>

/**
 * Model the natural environment.
 *
 * This class models the natural environment at a specific place and
 * time.  A separate instance is necessary for each location or time.
 *
 * This class should eventually move to SimGear.
 */
class FGEnvironment
{

public:

  FGEnvironment();
  FGEnvironment (const FGEnvironment &environment);
  virtual ~FGEnvironment();

  FGEnvironment & operator = ( const FGEnvironment & other );

  virtual void read (const SGPropertyNode * node);
  virtual void Tie( SGPropertyNode_ptr base, bool setArchivable = true );
  virtual void Untie();

  virtual time_t getLastRequestTime() const { return lastRequestTime; }
  virtual time_t getLastUpdateTime() const { return lastUpdateTime; }

  virtual void setLastRequestTime(time_t t) { lastRequestTime = t; }
  virtual void setLastUpdateTime(time_t t) { lastUpdateTime = t; }

  virtual void setGeod(const SGGeod& geod)
  {
	  mPosition = geod;
      set_latitude_deg(geod.getLatitudeDeg());
      set_longitude_deg(geod.getLongitudeDeg());
  }

  virtual const SGGeod& geod() const
  {
	  return mPosition;
  }
  virtual double get_visibility_m () const;

  virtual double get_temperature_sea_level_degc () const;
  virtual double get_temperature_degc () const;
  virtual double get_temperature_degf () const;
  virtual double get_dewpoint_sea_level_degc () const;
  virtual double get_dewpoint_degc () const;
  virtual double get_pressure_sea_level_inhg () const;
  virtual double get_pressure_inhg () const;
  virtual double get_density_slugft3 () const;

  virtual double get_relative_humidity () const;
  virtual double get_density_tropo_avg_kgm3 () const;
  virtual double get_altitude_half_to_sun_m () const;
  virtual double get_altitude_tropo_top_m () const;

  virtual double get_wind_from_heading_deg () const;
  virtual double get_wind_speed_kt () const;
  virtual double get_wind_from_north_fps () const;
  virtual double get_wind_from_east_fps () const;
  virtual double get_wind_from_down_fps () const;

  virtual double get_turbulence_magnitude_norm () const;
  virtual double get_turbulence_rate_hz () const;

  virtual void set_visibility_m (double v);

  virtual void set_temperature_sea_level_degc (double t);
  virtual void set_temperature_degc (double t);
  virtual void set_dewpoint_sea_level_degc (double d);
  virtual void set_dewpoint_degc (double d);
  virtual void set_pressure_sea_level_inhg (double p);
  virtual void set_pressure_inhg (double p);

  virtual void set_wind_from_heading_deg (double h);
  virtual void set_wind_speed_kt (double s);
  virtual void set_wind_from_north_fps (double n);
  virtual void set_wind_from_east_fps (double e);
  virtual void set_wind_from_down_fps (double d);

  virtual void set_turbulence_magnitude_norm (double t);
  virtual void set_turbulence_rate_hz (double t);

  virtual void set_latitude_deg(double lat);
  virtual void set_longitude_deg(double lon);

  virtual double get_latitude_deg() const;
  virtual double get_longitude_deg() const;

  virtual double get_elevation_ft () const;
  virtual void set_elevation_ft (double elevation_ft);
  virtual void set_altitude_half_to_sun_m (double alt);
  virtual void set_altitude_tropo_top_m (double alt);

  virtual bool set_live_update(bool live_update);

  virtual void set_is_isa(bool isa);
  virtual bool get_is_isa() const;

  FGEnvironment & interpolate (const FGEnvironment & env2, double fraction, FGEnvironment * result) const;

private:
  virtual void copy (const FGEnvironment &environment);
  void _init();
  void _recalc_hdgspd ();

  void _recalc_sl_temperature ();
  void _recalc_sl_dewpoint ();
  void _recalc_sl_pressure ();

  void _recalc_density_tropo_avg_kgm3 ();
  void _recalc_ne ();
  void _recalc_alt_dewpoint ();
  void _recalc_density ();
  void _recalc_relative_humidity ();
  void _recalc_alt_pt ();

  SGGeod mPosition;
  time_t lastRequestTime;
  time_t lastUpdateTime;
  double elevation_ft;
  double visibility_m;

  // Atmosphere
  double temperature_sea_level_degc;
  double temperature_degc;
  double dewpoint_sea_level_degc;
  double dewpoint_degc;
  double pressure_sea_level_inhg;
  double pressure_inhg;
  double density_slugft3;

  double density_tropo_avg_kgm3;
  double relative_humidity;
  double altitude_half_to_sun_m;
  double altitude_tropo_top_m;

  double turbulence_magnitude_norm;
  double turbulence_rate_hz;

  double wind_from_heading_deg;
  double wind_speed_kt;

  double wind_from_north_fps;
  double wind_from_east_fps;
  double wind_from_down_fps;

  bool     live_update;
  bool is_isa;
  simgear::TiedPropertyList _tiedProperties;
};
