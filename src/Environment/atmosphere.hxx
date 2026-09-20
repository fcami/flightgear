// atmosphere.hxx -- routines to model the air column
//
// Written by David Megginson, started February 2002.
// Modified by John Denker to correct physics errors in 2007
//
// SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/compiler.h>
#include <simgear/math/interpolater.hxx>

#include <cmath>
#include <utility>

/**
 * Model the atmosphere in a way consistent with the laws
 * of physics.
 *
 * Each instance of this class models a particular air mass.
 * You may freely move up, down, or sideways in the air mass.
 * In contrast, if you want to compare different air masses,
 * you should use a separate instance for each one.
 *
 * See also ./environment.hxx
 */

#define SCD(name,val) const double name(val)
namespace atmodel {
    SCD(g, 9.80665);		// [m/s/s] acceleration of gravity
    SCD(mm, .0289644);		// [kg/mole] molar mass of air (dry?)
    SCD(Rgas, 8.31432);		// [J/K/mole] gas constant
    SCD(inch, 0.0254);		// [m] definition of inch
    SCD(foot, 12 * inch);	// [m]
    SCD(inHg, 101325.0 / 760 * 1000 * inch);  // [Pa] definition of inHg
    SCD(mbar, 100.);            // [Pa] definition of millibar
    SCD(freezing, 273.15);	// [K] centigrade - kelvin offset
    SCD(nm, 1852);		// [m] nautical mile (NIST)
    SCD(sm, 5280*foot);		// [m] nautical mile (NIST)

    namespace ISA {
        SCD(P0, 101325.0); 		// [pascals] ISA sea-level pressure
        SCD(T0, 15. + freezing);	// [K] ISA sea-level temperature
        SCD(lam0, .0065);		// [K/m] ISA troposphere lapse rate
    }
}
#undef SCD



class ISA_layer {
public:
  double height;
  double temp;
  double lapse;
  ISA_layer(int, double h, double, double, double, double t, double,
                    double l=-1, double=0)
   :  height(h),        // [meters]
      temp(t),          // [kelvin]
      lapse(l)          // [K/m]
  {}
};

extern const ISA_layer ISA_def[];

std::pair<double, double> PT_vs_hpt(
    const double hh,
    const double _p0 = atmodel::ISA::P0,
    const double _t0 = atmodel::ISA::T0);

double P_layer(const double height, const double href,
  const double Pref, const double Tref, const double lapse );

double T_layer(const double height, const double href,
  const double Pref, const double Tref,  const double lapse );

// The base class is little more than a namespace.
// It has no constructor, no destructor, and no variables.
class FGAtmo {
public:
    double a_vs_p(const double press, const double qnh = atmodel::ISA::P0);
    double fake_T_vs_a_us(const double h_ft,
                          const double Tsl = atmodel::ISA::T0) const;
    double fake_dp_vs_a_us(const double dpsl, const double h_ft);
    void check_one(const double height);

    // Altimeter setting _in pascals_
    //  ... caller gets to convert to inHg or millibars
    // Field elevation in m
    // Field pressure in pascals
    // Valid for fields within the troposphere only.
    double QNH(const double field_elev, const double field_press);
    /**
 * Invert the QNH calculation to get the field pressure from a metar
 * report. Valid for fields within the troposphere only.
 * @param field_elev field elevation in m
 * @param qnh altimeter setting in pascals
 * @return field pressure _in pascals_. Caller gets to convert to inHg
 * or millibars
 */
    static double fieldPressure(const double field_elev, const double qnh);

    /**
    Compute the outside temperature at an altitude, according to the standard atmosphere
     model. Optionally allow offsetting the temperature at seal level, but default to the ISA standard
     for that value as well
     */
    static double ISATemperatureKAtAltitudeFt(const double alt,
                                             const double Tsl = atmodel::ISA::T0);

    /**
        Compute the speed of sound at an altitude
     */
    static double CSMetersPerSecondAtAltitudeFt(const double alt,
                                 const double Tsl = atmodel::ISA::T0);

    static double densityAtAltitudeFt(const double alt, const double Tsl = atmodel::ISA::T0);

    static double machFromKnotsAtAltitudeFt(const double knots,
                                                    const double altFt,
                                                    const double Tsl = atmodel::ISA::T0);

    static double knotsFromMachAtAltitudeFt(const double mach,
                                            const double altFt,
                                            const double Tsl = atmodel::ISA::T0);

    /**
     * Compute the sea-level temperature corresponding to an outside-air
     * temperature observed at a given field elevation. When is_isa is true,
     * the ISA sea-level temperature (15C) is assumed instead, and the
     * returned temperature-at-elevation is adjusted to match it.
     *
     * Valid for fields within the troposphere only.
     *
     * @param elevation_ft field elevation in feet
     * @param temperature_degc outside-air temperature at the field, in degrees C
     * @param is_isa use the ISA standard sea-level temperature instead of deriving it
     * @return {sea-level temperature degC, temperature-at-elevation degC}
     */
    static std::pair<double, double> seaLevelAndFieldTemperatureDegC(
        const double elevation_ft, const double temperature_degc, const bool is_isa);

    /**
     * Compute the sea-level dewpoint corresponding to a dewpoint observed at
     * a given field elevation, clamped so it never exceeds the sea-level
     * temperature.
     *
     * @param elevation_ft field elevation in feet
     * @param dewpoint_degc dewpoint at the field, in degrees C
     * @param sea_level_temperature_degc sea-level temperature, in degrees C,
     *        used to clamp the result (see seaLevelAndFieldTemperatureDegC)
     */
    static double seaLevelDewpointDegC(const double elevation_ft,
                                       const double dewpoint_degc,
                                       const double sea_level_temperature_degc);

    /**
     * Compute the sea-level pressure corresponding to a pressure observed
     * at a given field elevation and temperature.
     *
     * Valid for fields within the troposphere only.
     *
     * @param elevation_ft field elevation in feet
     * @param pressure_inhg field pressure, in inches of mercury
     * @param temperature_degc field temperature, in degrees C
     */
    static double seaLevelPressureInHg(const double elevation_ft,
                                       const double pressure_inhg,
                                       const double temperature_degc);

    /**
     * Compute relative humidity (percent) from temperature and dewpoint.
     * Not altitude-dependent: valid at any single level, as long as
     * temperature and dewpoint are from that same level.
     */
    static double relativeHumidityPercent(const double temperature_degc,
                                          const double dewpoint_degc);
};



class FGAtmoCache : FGAtmo {
    friend class FGAltimeter;
    SGInterpTable * a_tvs_p;	// _tvs_ means "tabulated versus"

public:
    FGAtmoCache();
    ~FGAtmoCache();
    void tabulate();
    void cache();
    void check_model();  // debug
};



class FGAltimeter : public FGAtmoCache {
public:
    FGAltimeter();
    double reading_ft(const double p_inHg,
            const double set_inHg = atmodel::ISA::P0/atmodel::inHg);
    inline double press_alt_ft(const double p_inHg) {
        return a_tvs_p->interpolate(p_inHg);
    }
    inline double kollsman_ft(const double set_inHg) {
        return a_tvs_p->interpolate(set_inHg);
    }

    // debug
    void dump_stack();
    void dump_stack1(const double Tref);
};
