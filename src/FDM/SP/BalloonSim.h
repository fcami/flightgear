// SPDX-FileCopyrightText: 1999 Christian Mayer <fgfs@christianmayer.de>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Header for the hot air balloon simulator
 */

#pragma once

/****************************************************************************/
/* INCLUDES								    */
/****************************************************************************/
#include <simgear/math/SGMath.hxx>

/****************************************************************************/
/* DEFINES								    */
/****************************************************************************/

/****************************************************************************/
/* CLASS DECLARATION							    */
/****************************************************************************/
class balloon
{
private:
    float dt;				//in s

    SGVec3f gravity_vector;		//in m/s*s
    SGVec3f hpr;                //the balloon isn't always exactly vertical (e.g. during gusts); normalized
    SGVec3f velocity;			//current velocity; it gets iterated at each 'update'
    SGVec3f position;			//current position in lat/lon/alt

    float balloon_envelope_area;	//area of the envelope
    float balloon_envelope_volume;	//volume of the envelope

    float wind_facing_area_of_balloon;
    float wind_facing_area_of_basket;

    float cw_envelope;			//wind resistance of the envelope
    float cw_basket;			//wind resistance of the bakset

    //all weights in kg
    float weight_of_total_fuel;
    float weight_of_envelope;
    float weight_of_basket;		//weight of all the unmovable stuff such as the basket, the burner and the empty tanks
    float weight_of_cargo;		//passengers and anything left (e.g. sand bags that are thrown away to give additional lift)

    float fuel_left;			//as a percentage
    float max_flow_of_fuel_per_second; //in percent per second
    float current_burner_strength;

    float lambda;                   //waermeuebergangskoeffizient (heat transmission coefficient?!?) for the envelope
    float l_of_the_envelope;		//the thickness of the envelope (in m)

    float T; //temperature inside the balloon

    float ground_level;

public:
    balloon(); //constructor for initializing the balloon

    void update();			//dt = time in seconds since last call
    void set_burner_strength(const float bs);

    void getVelocity(SGVec3f& v) const;
    void setVelocity(const SGVec3f& v);

    void getPosition(SGVec3f& v) const;
    void setPosition(const SGVec3f& v);

    void getHPR(SGVec3f& angles) const; //the balloon isn't always exactly vertical
    void setHPR(const SGVec3f& angles); //the balloon isn't always exactly vertical

    void setGroundLevel(const float altitude);

    float getTemperature(void) const;
    float getFuelLeft(void) const;

    void set_dt(const float new_dt) { dt = new_dt; }
};

/****************************************************************************/
