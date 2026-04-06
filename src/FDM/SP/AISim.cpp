// SPDX-FileCopyrightText: 2016 Erik Hofman <erik@ehofman.com>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Interface to the AI Sim
 */

#include "AISim.hpp"

#include <fenv.h>

#include <cmath>
#include <limits>
#include <cstdio>

#include <fstream>
#include <sstream>
#include <streambuf>
#include <string>

#include <nlohmann/json.hpp>

#ifdef ENABLE_SP_FDM
# include <simgear/constants.h>
# include <simgear/math/simd.hxx>
# include <simgear/math/simd4x4.hxx>
# include <simgear/math/sg_geodesy.hxx>

#include <Aircraft/controls.hxx>
#include <FDM/flight.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#else
# include "simd.hxx"
# include "simd4x4.hxx"
#endif

#define FEET_TO_INCHES       12.0f
#define INCHES_TO_FEET       (1.0f/FEET_TO_INCHES)

FGAISim::FGAISim(double dt)
{
    simd4x4::zeros(xCDYLT);
    simd4x4::zeros(xClmnT);

    for (size_t i=0; i<AISIM_MAX; ++i)
    {
        contact_pos[i] = 0.0f;
        contact_spring[i] = contact_damp[i] = 0.0f;
        FT[i] = MT[i] = 0.0f;
    }

#ifdef ENABLE_SP_FDM
    SGPath aircraft_path( fgGetString("/sim/fg-root") );
    SGPropertyNode_ptr aero = fgGetNode("sim/aero", true);
    aircraft_path.append("Aircraft-aisim");
    aircraft_path.append(aero->getStringValue());
    aircraft_path.concat(".json");

    load(aircraft_path.str());
#else
    load("");
#endif

    set_rudder_norm(0.0f);
    set_elevator_norm(0.0f);
    set_aileron_norm(0.0f);
    set_throttle_norm(0.0f);
    set_flaps_norm(0.0f);
    set_brake_norm(0.0f);

    set_velocity_fps(0.0f);
    set_alpha_rad(0.0f);
    set_beta_rad(0.0f);

    /* useful constants assigned to a vector */
    xCp[SIDE] = CYp;
    xCr[SIDE] = CYr;
    xCp[ROLL] = Clp;
    xCr[ROLL] = Clr;
    xCp[YAW]  = Cnp;
    xCr[YAW]  = Cnr;

    xCq[LIFT] = CLq;
    xCadot[LIFT] = CLadot;

    xCq[PITCH] = Cmq;
    xCadot[PITCH] = Cmadot;

    xCDYLT.ptr()[MIN][LIFT] = CLmin;
    xCDYLT.ptr()[MIN][DRAG] = CDmin;

    /* m is assigned in the load function */
    inv_mass = aiVec3(1.0f)/mass;

    // calculate aircraft position when resting on the landing gear.
    float cg_agl = -cg[Z];
    if (no_contacts)
    {
        for (size_t i=0; i<no_contacts; ++i) {
            if (cg_agl < contact_pos[i][Z]) cg_agl = contact_pos[i][Z];
        }
    }
    set_altitude_agl_ft(cg_agl);

    // Contact point at the center of gravity
    contact_pos[no_contacts] = 0.0f;
    contact_spring[no_contacts] = -20000.0f;
    contact_damp[no_contacts] = -2000.0f;
    no_contacts++;

    aiMtx4 mcg;
    simd4x4::unit(mcg);
    simd4x4::translate(mcg, cg);

    mJ = aiMtx4( I[XX],  0.0f, -I[XZ], 0.0f,
                  0.0f, I[YY],   0.0f, 0.0f,
                -I[XZ],  0.0f,  I[ZZ], 0.0f,
                  0.0f,  0.0f,   0.0f, 0.0f);
    mJinv = invert_inertia(mJ);
    mJ *= mcg;
    mJinv *= matrix_inverse(mcg);
}

FGAISim::~FGAISim()
{
}

// Initialize the AISim flight model, dt is the time increment for
// each subsequent iteration through the EOM
void
FGAISim::init()
{
//  feenableexcept(FE_INVALID | FE_OVERFLOW);
#ifdef ENABLE_SP_FDM
    // do init common to all the FDM's
    common_init();

    // now do init specific to the AISim
    SG_LOG( SG_FLIGHT, SG_INFO, "Starting initializing AISim" );

    // cg_agl holds the lowest contact point of the aircraft
    set_Altitude( get_Altitude() + cg_agl );
    set_location_geod( get_Latitude(), get_Longitude(), get_Altitude() );
    set_euler_angles_rad( get_Phi(), get_Theta(), get_Psi() );
    set_velocity_fps( fgGetFloat("sim/presets/uBody-fps"),
                      fgGetFloat("sim/presets/vBody-fps"),
                      fgGetFloat("sim/presets/wBody-fps"));
#endif
}

void
FGAISim::update(double ddt)
{
#ifdef ENABLE_SP_FDM
    if (is_suspended() || ddt == 0)
        return;
#endif

    // initialize all of AISim vars
    aiVec3 dt(ddt);

#ifdef ENABLE_SP_FDM
    copy_to_AISim();
#endif

    /* --------------------------------------------------------------------
     * Earth-to-Body-Axis Transformation Matrix (ZYX Euler sequence)
     * Rx(phi)*Ry(theta)*Rz(psi):
     *
     *  | cθ·cψ               cθ·sψ              -sθ   |
     *  | sφ·sθ·cψ - cφ·sψ    sφ·sθ·sψ + cφ·cψ   sφ·cθ |
     *  | cφ·sθ·cψ + sφ·sψ    cφ·sθ·sψ - sφ·cψ   cφ·cθ |
     *
     * Trig values computed once and reused in the Euler-rate kinematics.
     * ------------------------------------------------------------------ */
    float sphi = std::sin(euler[PHI]), cphi = std::cos(euler[PHI]);
    float sthe = std::sin(euler[THETA]), cthe = std::cos(euler[THETA]);
    float spsi = std::sin(euler[PSI]), cpsi = std::cos(euler[PSI]);

    aiMtx4 mNed2Body(
        cthe * cpsi, cthe * spsi, -sthe, 0.0f,
        sphi * sthe * cpsi - cphi * spsi, sphi * sthe * spsi + cphi * cpsi, sphi * cthe, 0.0f,
        cphi * sthe * cpsi + sphi * spsi, cphi * sthe * spsi - sphi * cpsi, cphi * cthe, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    aiMtx4 mBody2Ned = simd4x4::transpose(mNed2Body);
    aiVec3 wind = mNed2Body*wind_ned;

    /* Air-Relative velocity vector */
    vUVWaero = vUVW + wind;
    update_velocity( simd4::magnitude( vUVWaero ) );

    /* Wind angles */
    aiVec3 prevAOA = AOA;
    alpha = (vUVWaero[U] == 0.0f) ? 0.0f : std::atan2(vUVWaero[W], vUVWaero[U]);
    set_alpha_rad( alpha );

    beta = (velocity == 0.0f) ? 0.0f : std::asin(vUVWaero[V]/velocity);
    set_beta_rad( beta );

    /* set_alpha_rad and set_beta_rad set the new AOA */
    AOAdot = (AOA - prevAOA)/dt;

    /* Force and Moment Coefficients */
    /* Sum all Drag, Side, Lift, Roll, Pitch and Yaw and Thrust coefficients */
    /* Rate terms: (xCq*q + xCadot*adot)*cbar_2U  and
     *             (xCp*p + xCr*r)*b_2U
     * vPQR = {p,q,r}.  Broadcast each component as a vector multiply. */
    aiVec4 Ccbar2U = (xCq * vPQR[Q] + xCadot * AOAdot[ALPHA]) * cbar_2U;
    aiVec4 Cb2U = (xCp * vPQR[P] + xCr * vPQR[R]) * b_2U;

    /* xCDYLT and xClmnT already have their factors applied */
    /* in the functions in the header file.                 */
    aiVec4 CDYL(0.0f, Cb2U[SIDE], Ccbar2U[LIFT]);
    aiVec4 Clmn(Cb2U[ROLL], Ccbar2U[PITCH], Cb2U[YAW]);
    size_t i = 3;
    do {
        CDYL += static_cast<aiVec4>(xCDYLT.m4x4()[i]);
        Clmn += static_cast<aiVec4>(xClmnT.m4x4()[i]);
    }
    while(i--);

    float CL = CDYL[LIFT];
    CDYL += aiVec3(CDi * CL * CL, 0.0f, 0.0f);

    /* State Accelerations (convert coefficients to forces and moments) */
    aiVec3 FDYL = CDYL*Coef2Force;
    aiVec3 Mlmn = Clmn*Coef2Moment;

    /* convert from wind axes to body axes */
    /* Ry(alpha)*Rz(-beta) built as an aiMtx4 from pre-computed
     * trig scalars, then applied as a single matrix-vector multiply.
     *
     *  | ca*cb   -sb   -sa*cb |   | Fdrag |
     *  | ca*sb    cb   -sa*sb | * | Fside |
     *  |    sa     0      ca  |   | Flift |
     */
    float ca = std::cos(alpha), sa = std::sin(alpha);
    float cb = std::cos(beta), sb = std::sin(beta);

    aiMtx4 mWind2Body(
        ca * cb, -sb, -sa * cb, 0.0f,
        ca * sb, cb, -sa * sb, 0.0f,
        sa, 0.0f, ca, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    aiVec3 FXYZ_body = mWind2Body*FDYL;

    aiVec3 gravity_body = mNed2Body*gravity_ned;
    FXYZ_body += gravity_body*mass;

    /* Thrust */
    float Cth = rho*throttle*throttle;
    for (i = 0; i < no_engines; i++) {
        aiVec3 FEngine = FT[i]*(Cth*n2[i]);
        aiVec3 MEngine = MT[i]*Cth;

        FXYZ_body += FEngine;
        Mlmn += MEngine;
    }

    /* contact point (landing gear) forces and moments */
    WoW = false;
    if (no_contacts && cg_agl < 10.0f)
    {
        size_t WoW_main = 0;
        i = 0;
        do
        {
            aiVec3 lg_ground_ned = mBody2Ned*contact_pos[i];
            if (lg_ground_ned[Z] > cg_agl) // // weight on wheel
            {
                /* Compression depth = how far the contact point has penetrated
                 * below the ground surface. lg_ground_ned[Z] is the total depth
                 * of the contact point below the CG; cg_agl is the CG height
                 * above ground at rest. The difference is the actual strut
                 * compression. Using lg_ground_ned[Z] alone overcalculates Fn
                 * and makes it vary with attitude rather than true compression.
                 */
                float compression = lg_ground_ned[Z] - cg_agl;

                aiVec3 lg_vrot = simd4::cross(vPQR, contact_pos[i]);
                aiVec3 lg_cg_vned = mBody2Ned*lg_vrot;
                aiVec3 lg_vned = vNED + lg_cg_vned;
                float Fn = std::min((contact_spring[i] * compression +
                                     contact_damp[i] * lg_vned[Z]),
                                    0.0f);

                aiVec3 Fcontact_ned(0.0f, 0.0f, Fn);
                aiVec3 Fgear = mNed2Body * Fcontact_ned;
                FXYZ_body += Fgear;

                /* Moment arm from CG to contact point (both in body frame) */
                aiVec3 arm = contact_pos[i] - cg;
                Mlmn += simd4::cross(arm, Fgear);

                // only apply friction when there is a noticeable velocity
                float vground = simd4::magnitude(aiVec2(lg_vned));
                if (vground > 0.001f) {
                    /* Friction in body frame: scale the normal force magnitude
                    * by mu and the normalised contact-point body velocity so
                    * the force opposes motion and is proportional to speed.
                    * Use the body-frame contact velocity (lg_vrot gives the
                    * rotational contribution;
                    * full body velocity is vUVW + lg_vrot).
                    */
                    aiVec3 lg_vbody = vUVW + lg_vrot;
                    float vbody_mag = simd4::magnitude(lg_vbody);
                    if (vbody_mag > 0.001f) {
                        /* mu_body = {rolling_mu, side_mu, 0}; Fn is negative so
                        * -Fn gives the positive normal load magnitude. */
                        aiVec3 Fbrake = (mu_body * (-Fn)) * (lg_vbody * (1.0f / vbody_mag));
                        FXYZ_body += Fbrake;
                        Mlmn += simd4::cross(arm, Fbrake);
                    }
                }
                if (i<3) WoW_main++;
            }
        }
        while(++i < no_contacts);
        WoW = (WoW_main == 3);
    }

    /* local body accelrations */
    XYZdot = FXYZ_body * inv_mass;


    /* Dynamic Equations */

    /* body-axis translational accelerations: forward, sideward, upward */
    vUVWdot = XYZdot - simd4::cross(vPQR, vUVW);
    vUVW += vUVWdot*dt;

    /* body-axis rotational accelerations: rolling, pitching, yawing */
    vPQRdot = mJinv*(Mlmn - vPQR*(mJ*vPQR));
    vPQR += vPQRdot * dt;

    /* position of center of mass wrt earth: north, east, down */
    vNED = mBody2Ned*vUVW;
    aiVec3 NEDdist = vNED * dt;

#ifdef ENABLE_SP_FDM
    double dist = simd4::magnitude( aiVec2(NEDdist) );
    double ground_track_deg = std::atan2(vNED[EAST], vNED[NORTH]) * SGD_RADIANS_TO_DEGREES;
    double lat2 = 0.0, lon2 = 0.0, az2 = 0.0;
    geo_direct_wgs_84(0.0, location_geod[LATITUDE] * SGD_RADIANS_TO_DEGREES,
                      location_geod[LONGITUDE] * SGD_RADIANS_TO_DEGREES,
                      ground_track_deg,
                      dist * SG_FEET_TO_METER, &lat2, &lon2, &az2);
    set_location_geod( lat2 * SGD_DEGREES_TO_RADIANS,
                       lon2 * SGD_DEGREES_TO_RADIANS,
                       location_geod[ALTITUDE] - NEDdist[DOWN] );
//  set_heading_rad( az2 * SGD_DEGREES_TO_RADIANS );
#else
    location_geod[X] += NEDdist[X];
    location_geod[Y] += NEDdist[Y];
    location_geod[Z] -= NEDdist[Z];
    set_altitude_cg_agl_ft(location_geod[DOWN]);
#endif

    /* angle of body wrt earth: phi (roll), theta (pitch), psi (heading) */
    /* Reuse sinEuler/cosEuler computed above for the NED to Body matrix.
     * No additional sin/cos calls needed here.
     *
     * euler_dot[PSI]   = (q*sin(phi) + r*cos(phi)) / cos(theta)
     * euler_dot[THETA] =  q*cos(phi) - r*sin(phi)
     * euler_dot[PHI]   =  p + euler_dot[PSI]*sin(theta)
     */
    float cthe_safe = cthe;
    if (std::abs(cthe_safe) < 0.00001f)
        cthe_safe = std::copysign(0.00001f, cthe_safe);

    float psi_dot = (vPQR[Q] * sphi + vPQR[R] * cphi) / cthe_safe;
    euler_dot[PSI] = psi_dot;
    euler_dot[THETA] = vPQR[Q] * cphi - vPQR[R] * sphi;
    euler_dot[PHI] = vPQR[P] + psi_dot * sthe;

    euler += euler_dot * dt;

#ifdef ENABLE_SP_FDM
    copy_from_AISim();
#endif
}

#ifdef ENABLE_SP_FDM
bool
FGAISim::copy_to_AISim()
{
    set_rudder_norm(globals->get_controls()->get_rudder());
    set_elevator_norm(globals->get_controls()->get_elevator());
    set_aileron_norm(globals->get_controls()->get_aileron());
    set_flaps_norm(globals->get_controls()->get_flaps());
    set_throttle_norm(globals->get_controls()->get_throttle(0));
    set_brake_norm(0.5f*(globals->get_controls()->get_brake_left()
                         +globals->get_controls()->get_brake_right()));


    set_altitude_asl_ft(get_Altitude());
    set_altitude_agl_ft(get_Altitude_AGL());

//  set_location_geod(get_Latitude(), get_Longitude(), altitde);
//  set_velocity_fps(get_V_calibrated_kts());

    return true;
}

bool
FGAISim::copy_from_AISim()
{
    // Mass properties and geometry values
//  _set_Inertias( mass, I[XX], I[YY], I[ZZ], I[XZ] );
    _set_CG_Position( cg[X], cg[Y], cg[Z]);

    // Accelerations
    _set_Accels_Body( vUVWdot[U], vUVWdot[V], vUVWdot[W] );

    // Velocities
    _set_V_equiv_kts( velocity*std::sqrt(sigma) * SG_FPS_TO_KT );
    _set_V_calibrated_kts( std::sqrt( 2.0f*qbar*sigma/rho) * SG_FPS_TO_KT );
    _set_V_ground_speed( simd4::magnitude(aiVec2(vNED)) * SG_FPS_TO_KT );
    _set_Mach_number( mach );

    _set_Velocities_Local( vNED[NORTH], vNED[EAST], vNED[DOWN] );
//  _set_Velocities_Local_Airmass( vUVWaero[U], vUVWaero[V], vUVWaero[W] );
    _set_Velocities_Body( vUVW[U], vUVW[V], vUVW[W] );
    _set_Omega_Body( vPQR[P], vPQR[Q], vPQR[R] );
    _set_Euler_Rates( euler_dot[PHI], euler_dot[THETA], euler_dot[PSI] );

    // Positions
    double lon = location_geod[LONGITUDE];
    double lat_geod = location_geod[LATITUDE];
    double altitude = location_geod[ALTITUDE];
    double lat_geoc;

    sgGeodToGeoc( lat_geod, altitude, &sl_radius, &lat_geoc );
    _set_Geocentric_Position( lat_geoc, lon, sl_radius+altitude );

    _set_Geodetic_Position( lat_geod, lon, altitude );

    _set_Euler_Angles( euler[PHI], euler[THETA],
                       SGMiscd::normalizePeriodic(0, SGD_2PI, euler[PSI]) );

    set_altitude_agl_ft( altitude - get_Runway_altitude() );
    _set_Altitude_AGL( cg_agl );

//  _set_Alpha( alpha );
//  _set_Beta(  beta );

//  _set_Gamma_vert_rad( Gamma_vert_rad );

//  _set_Density( Density );

//  _set_Static_pressure( Static_pressure );

//  _set_Static_temperature( Static_temperature );

    _set_Sea_level_radius( sl_radius * SG_METER_TO_FEET );
//  _set_Earth_position_angle( Earth_position_angle );

//  _set_Runway_altitude( get_Runway_altitude() );

    _set_Climb_Rate( -vNED[DOWN] );

    _update_ground_elev_at_pos();

    return true;
}
#endif

// ----------------------------------------------------------------------------

#define OMEGA_EARTH 	0.00007272205217f
#define MAX_ALT		101

// 1976 Standard Atmosphere - Density (slugs/ft2): 0 - 101,000 ft
float FGAISim::density[MAX_ALT] = {
   0.0023771699, 0.0023083901, 0.0022411400, 0.0021753900, 0.0021111399,
   0.0020483399, 0.0019869800, 0.0019270401, 0.0018685000, 0.0018113200,
   0.0017554900, 0.0017009900, 0.0016477900, 0.0015958800, 0.0015452200,
   0.0014958100, 0.0014476100, 0.0014006100, 0.0013547899, 0.0013101200,
   0.0012665900, 0.0012241700, 0.0011828500, 0.0011426000, 0.0011034100,
   0.0010652600, 0.0010281201, 0.0009919840, 0.0009568270, 0.0009226310,
   0.0008893780, 0.0008570500, 0.0008256280, 0.0007950960, 0.0007654340,
   0.0007366270, 0.0007086570, 0.0006759540, 0.0006442340, 0.0006140020,
   0.0005851890, 0.0005577280, 0.0005315560, 0.0005066120, 0.0004828380,
   0.0004601800, 0.0004385860, 0.0004180040, 0.0003983890, 0.0003796940,
   0.0003618760, 0.0003448940, 0.0003287090, 0.0003132840, 0.0002985830,
   0.0002845710, 0.0002712170, 0.0002584900, 0.0002463600, 0.0002347990,
   0.0002237810, 0.0002132790, 0.0002032710, 0.0001937320, 0.0001846410,
   0.0001759760, 0.0001676290, 0.0001595480, 0.0001518670, 0.0001445660,
   0.0001376250, 0.0001310260, 0.0001247530, 0.0001187880, 0.0001131160,
   0.0001077220, 0.0001025920, 0.0000977131, 0.0000930725, 0.0000886582,
   0.0000844590, 0.0000804641, 0.0000766632, 0.0000730467, 0.0000696054,
   0.0000663307, 0.0000632142, 0.0000602481, 0.0000574249, 0.0000547376,
   0.0000521794, 0.0000497441, 0.0000474254, 0.0000452178, 0.0000431158,
   0.0000411140, 0.0000392078, 0.0000373923, 0.0000356632, 0.0000340162,
   0.0000324473
};

// 1976 Standard Atmosphere - Speed of sound (ft/s): 0 - 101,000 ft
float FGAISim::vsound[MAX_ALT] = {
   1116.450, 1112.610, 1108.750, 1104.880, 1100.990, 1097.090, 1093.180,
   1089.250, 1085.310, 1081.360, 1077.390, 1073.400, 1069.400, 1065.390,
   1061.360, 1057.310, 1053.250, 1049.180, 1045.080, 1040.970, 1036.850,
   1032.710, 1028.550, 1024.380, 1020.190, 1015.980, 1011.750, 1007.510,
   1003.240,  998.963,  994.664,  990.347,  986.010,  981.655,  977.280,
    972.885,  968.471,  968.076,  968.076,  968.076,  968.076,  968.076,
    968.076,  968.076,  968.076,  968.076,  968.076,  968.076,  968.076,
    968.076,  968.076,  968.076,  968.076,  968.076,  968.076,  968.076,
    968.076,  968.076,  968.076,  968.076,  968.076,  968.076,  968.076,
    968.076,  968.076,  968.076,  968.337,  969.017,  969.698,  970.377,
    971.056,  971.735,  972.413,  973.091,  973.768,  974.445,  975.121,
    975.797,  976.472,  977.147,  977.822,  978.496,  979.169,  979.842,
    980.515,  981.187,  981.858,  982.530,  983.200,  983.871,  984.541,
    985.210,  985.879,  986.547,  987.215,  987.883,  988.550,  989.217,
    989.883,  990.549,  991.214
};

void
FGAISim::update_velocity(float v)
{
    velocity = v;

    /* altitude related */
    float alt_kft = _MINMAX(location_geod[ALTITUDE]/1000.0f, 0, MAX_ALT);
    float alt_idx = std::floor(alt_kft);
    size_t idx = static_cast<size_t>(alt_idx);
    float fract = alt_kft - alt_idx;
    float ifract = 1.0f - fract;

    /* linear interpolation for density */
    rho = ifract*density[idx] + fract*density[idx+1];
    qbar = 0.5f*rho*v*v;
    sigma = rho/density[0];

    float Sqbar = Sw*qbar;
    float Sbqbar = Sqbar*span;
    float Sqbarcbar = Sqbar*cbar;

    // Drag opposes forward motion (-X body axis) and lift acts upward
    // (-Z, since body Z is positive-downward). Side force is +Y.
    // All-positive Sqbar would invert drag and lift, making drag propulsive
    // and swamping aileron/rudder authority.
    Coef2Force[DRAG] = -Sqbar;
    Coef2Force[SIDE] = Sqbar;
    Coef2Force[LIFT] = -Sqbar;
    Coef2Moment = aiVec3(Sbqbar);
    Coef2Moment[PITCH] = Sqbarcbar;

    /* linear interpolation for speed of sound */
    float vs = ifract*vsound[idx] + fract*vsound[idx+1];
    mach = v/vs;

    /*  useful semi-constants */
    if (v == 0.0f) {
        cbar_2U = b_2U = 0.0f;
    }
    else
    {
        b_2U = 0.5f*span/v;
        cbar_2U = 0.5f*cbar/v;
    }
}

// structural: x is pos. aft., y is pos. right, z is pos. up. in inches.
// body:       x is pos. fwd., y is pos. right, z is pos. down in feet.
void
FGAISim::struct_to_body(aiVec3 &pos)
{
    pos *= INCHES_TO_FEET;
    pos[0] = -pos[0];
    pos[2] = -pos[2];
}

simd4x4_t<float,4>
FGAISim::matrix_inverse(aiMtx4 mtx)
{
    aiMtx4 dst;
    aiVec4 v1, v2;

    dst = simd4x4::transpose(mtx);

    v1 = static_cast<aiVec4>(mtx.m4x4()[3]);
    v2 = static_cast<aiVec4>(mtx.m4x4()[0]);
    dst.ptr()[3][0] = -simd4::dot(v1, v2);

    v2 = static_cast<aiVec4>(mtx.m4x4()[1]);
    dst.ptr()[3][1] = -simd4::dot(v1, v2);

    v2 = static_cast<aiVec4>(mtx.m4x4()[2]);
    dst.ptr()[3][2] = -simd4::dot(v1, v2);

    return dst;
}

simd4x4_t<float,4>
FGAISim::invert_inertia(aiMtx4 mtx)
{
    float Ixx, Iyy, Izz, Ixz;
    float k1, k3, k4, k6;
    float denom;

    Ixx = mtx.ptr()[0][0];
    Iyy = mtx.ptr()[1][1];
    Izz = mtx.ptr()[2][2];
    Ixz = -mtx.ptr()[0][2];

    k1 = Iyy*Izz;
    k3 = Iyy*Ixz;
    denom = 1.0f/(Ixx*k1 - Ixz*k3);

    k1 *= denom;
    k3 *= denom;
    k4 = (Izz*Ixx - Ixz*Ixz)*denom;
    k6 = Ixx*Iyy*denom;

    return aiMtx4(   k1, 0.0f,   k3, 0.0f,
                   0.0f,   k4, 0.0f, 0.0f,
                     k3, 0.0f,   k6, 0.0f,
                   0.0f, 0.0f, 0.0f, 0.0f );
}


std::map<std::string, float>
FGAISim::jsonParse(std::istream& in)
{
    using nj = nlohmann::json;
    const auto json = nj::parse(in, nullptr, false);
    if (json.is_discarded()) {
        SG_LOG(SG_FLIGHT, SG_ALERT, "Failed to parse Aircraft Json");
        return {};
    }

    jsonMap rv;
    for (const auto& i : json.items()) {
        const auto& v = i.value();
        if (v.is_number()) {
            // simple case, top level scalar numerical value
            rv.emplace(i.key(), v.template get<float>());
        } else if (v.is_array()) {
            int index = 0;
            for (const auto& child : v) {
                std::string k = i.key() + "[" + std::to_string(index++) + "]";
                if (child.is_object()) {
                    // child is an object, we will iterate its children and add them below
                    // a path separator to the result map
                    for (const auto& subchild : child.items()) {
                        const auto subChildK = k + "/" + subchild.key();
                        if (subchild.value().is_array()) {
                            // subchild is itself an array ...
                            int subChildIndex = 0;
                            for (const auto& subChildElement : subchild.value()) {
                                std::string k2 = subChildK + "[" + std::to_string(subChildIndex++) + "]";
                                rv.emplace(k2, subChildElement.template get<float>());
                            }
                        } else {
                            // simple value
                            rv.emplace(subChildK, subchild.value().template get<float>());
                        }
                    }
                } else {
                    rv.emplace(k, child.template get<float>());
                }
            } // of array iteration
        }
    }

    return rv;
}

bool
FGAISim::load(std::string path)
{
    std::ifstream file(path);
    std::string jsonString;

    file.seekg(0, std::ios::end);
    jsonString.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    jsonString.assign((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    // Parse from the string we just read -- the file stream is at EOF here
    // and passing it directly to jsonParse would yield an empty map.
    std::istringstream jsonStream(jsonString);
    jsonMap data = jsonParse(jsonStream);

    Sw   = data["Sw"];
    cbar = data["cbar"];
    span = data["bw"]; // JSON key is "bw", not "b"

    mass = data["mass"]/AISIM_G;

    I[XX] = data["Ixx"];
    I[YY] = data["Iyy"];
    I[ZZ] = data["Izz"];
    I[XZ] = data["Ixz"];

    // Center of gravity, gears and engines are in the structural frame
    //  positions are in inches
    //  0: X-axis is directed afterwards,
    //  1: Y-axis is directed towards the right,
    //  2: Z-axis is directed upwards
    //
    // c.g. is relative to the aero reference point
    cg[X] = data["cg[0]"];
    cg[Y] = data["cg[1]"];
    cg[Z] = data["cg[2]"];
    struct_to_body(cg);

    // Gear ground contact points relative to center of gravity.
    no_contacts = 0;
    do
    {
        size_t i = no_contacts;
        std::string gearstr = "gear[" + std::to_string(i) + "]";

        float spring = data[gearstr + "/spring"];
        if (!spring) break;

        contact_pos[i][X] = data[gearstr + "/pos[0]"];
        contact_pos[i][Y] = data[gearstr + "/pos[1]"];
        contact_pos[i][Z] = data[gearstr + "/pos[2]"];
        struct_to_body(contact_pos[i]);

        contact_spring[i] = -data[gearstr + "/spring"];
        contact_damp[i] = -data[gearstr + "/damp"];
    }
    while (++no_contacts < AISIM_MAX);

    /* Thuster / propulsion locations relative to c.g. */
    no_engines = 0;
    do
    {
        size_t i = no_engines;
        std::string engstr = "engine[" + std::to_string(i) + "]";

        float FTmax = data[engstr + "/FT_max"];
        if (!FTmax) break;

        aiVec3 pos;
        pos[0] = data[engstr + "/pos[0]"];
        pos[1] = data[engstr + "/pos[1]"];
        pos[2] = data[engstr + "/pos[2]"];
        struct_to_body(pos);

        // Thruster orientation is in the following sequence: pitch, roll, yaw
        aiVec3 orientation(data[engstr + "/dir[0]"],  // pitch (degrees)
                           data[engstr + "/dir[1]"],  // roll (degrees)
                           data[engstr + "/dir[2]"]); // yaw (degrees)
        orientation *= SG_DEGREES_TO_RADIANS;

        aiVec3 dir;
        float len = simd4::magnitude(orientation);
        if (len > 0.0f) {
            orientation /= len; // normalize without altering len
            aiMtx4 mWind2Body = simd4x4::rotation_matrix(len, orientation);
            dir = mWind2Body * aiVec3(1.0f, 0.0f, 0.0f);
        } else {
            dir = aiVec3(1.0f, 0.0f, 0.0f);
        }
        aiVec3 rot = simd4::cross(pos, dir);

        float rho = 0.002379f;
//      float MTmax = data[engstr + "/MT_max"];

        float max_rpm = data[engstr + "/rpm_max"];
        if (max_rpm == 0.0f) {
             n2[i] = 1.0f;
        }
        else
        {
            n2[i] = max_rpm/60.0f;
            n2[i] *= n2[i];
        }

        FTmax /= (rho*n2[i]);
//      MTmax /= rho / max_rpm;

        FT[i] = dir * FTmax;
        MT[i] = rot * FT[i]; // + dir * MTmax;
    }
    while(++no_engines < AISIM_MAX);

    float de_max = data["de_max"]*SG_DEGREES_TO_RADIANS;
    float dr_max = data["dr_max"]*SG_DEGREES_TO_RADIANS;
    float da_max = data["da_max"]*SG_DEGREES_TO_RADIANS;
    float df_max = data["df_max"]*SG_DEGREES_TO_RADIANS;

    /* aerodynamic coefficients */
    CLmin  = data["CLmin"];
    CLa    = data["CLa"];
    CLadot = data["CLadot"];
    CLq    = data["CLq"];
    CLdf_n = data["CLdf"]*df_max;

    CDmin  = data["CDmin"];
    CDa    = data["CDa"];
    CDb    = data["CDb"];
    CDi    = data["CDi"];
    CDdf_n = data["CDdf"]*df_max;

    CYb    = data["CYb"];
    CYp    = data["CYp"];
    CYr    = data["CYr"];
    CYdr_n = data["CYdr"]*dr_max;

    Clb    = data["Clb"];
    Clp    = data["Clp"];
    Clr    = data["Clr"];
    Clda_n = data["Clda"]*da_max;
    Cldr_n = data["Cldr"]*dr_max;

    Cma    = data["Cma"];
    Cmadot = data["Cmadot"];
    Cmq    = data["Cmq"];
    Cmde_n = data["Cmde"]*de_max;
    Cmdf_n = data["Cmdf"]*df_max;

    Cnb = data["Cnb"];
    Cnp    = data["Cnp"];
    Cnr    = data["Cnr"];
    Cnda_n = data["Cnda"]*da_max;
    Cndr_n = data["Cndr"]*dr_max;

    return true;
}


// Register the subsystem.
#if 0
SGSubsystemMgr::Registrant<FGAISim> registrantFGAISim;
#endif
