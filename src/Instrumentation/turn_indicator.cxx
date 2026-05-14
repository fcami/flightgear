/*
 * SPDX-FileName: turn_indicator.cxx
 * SPDX-FileComment: an electric-powered turn indicator.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2002 David Megginson
 * SPDX-FileContributor:  Written by David Megginson, started 2002.
 * SPDX-FileContributor: Enhanced by Benedikt Hallinger, 2023
 */

#include "config.h"

#include "turn_indicator.hxx"

#include <simgear/compiler.h>
#include <iostream>
#include <string>
#include <sstream>

#include <Main/fg_props.hxx>
#include <Main/util.hxx>

using std::string;

// Use a bigger number to be more responsive, or a smaller number
// to be more sluggish.
#define RESPONSIVENESS 0.5


TurnIndicator::TurnIndicator ( SGPropertyNode *node) :
    _last_rate(0)
{
    if( !node->getBoolValue("new-default-power-path", 0) ){
       setDefaultPowerSupplyPath("/systems/electrical/outputs/turn-coordinator");
    }

    _max_out_degsec = node->getDoubleValue("max-indicated-degsec", 6.0);

    SGPropertyNode* gyro_cfg = node->getChild("gyro", 0, true);
    _gyro_spin_up = gyro_cfg->getDoubleValue("spin-up-sec", 4.0);
    _gyro_spin_down = gyro_cfg->getDoubleValue("spin-down-sec", 180.0);
    _gyro_spin_valid_from = gyro_cfg->getDoubleValue("gyro-spin-valid-norm", 0.93);

    readConfig(node, "turn-indicator");
}

TurnIndicator::~TurnIndicator ()
{
}

void
TurnIndicator::init ()
{
    string branch = nodePath();

    SGPropertyNode *node = fgGetNode(branch, true );
    _roll_rate_node = fgGetNode("/orientation/roll-rate-degps", true);
    _yaw_rate_node = fgGetNode("/orientation/yaw-rate-degps", true);
    _rate_out_node = node->getChild("indicated-turn-rate", 0, true);
    _is_valid_node = node->getChild("is-valid", 0, true);
    SGPropertyNode* gyro_node = node->getChild("gyro", 0, true);
    _spin_node = gyro_node->getChild("spin", 0.0, true);
    _gyro_spin_up_node = gyro_node->getChild("spin-up-sec", 0, true);
    _gyro_spin_down_node = gyro_node->getChild("spin-down-sec", 0, true);
    _gyro_spin_valid_from_node = gyro_node->getChild("gyro-spin-valid-norm", 0, true);
    if (!_gyro_spin_up_node->hasValue())
        _gyro_spin_up_node->setDoubleValue(_gyro_spin_up);
    if (!_gyro_spin_down_node->hasValue())
        _gyro_spin_down_node->setDoubleValue(_gyro_spin_down);
    if (!_gyro_spin_valid_from_node->hasValue())
        _gyro_spin_valid_from_node->setDoubleValue(_gyro_spin_valid_from);

    initServicePowerProperties(node);

    reinit();
}

void
TurnIndicator::reinit ()
{
    _last_rate = 0;
    _gyro.reinit();
}

void
TurnIndicator::update (double dt)
{
    // Get the spin from the gyro
    _gyro.set_power_norm(isServiceableAndPowered());
    _gyro.set_spin_up(_gyro_spin_up_node->getDoubleValue());
    _gyro.set_spin_down(_gyro_spin_down_node->getDoubleValue());
    _gyro.set_spin_norm(_spin_node->getDoubleValue());
    _gyro.update(dt);
    double spin = _gyro.get_spin_norm();
    _spin_node->setDoubleValue( spin );

    //Set 'indication is valid'
    _is_valid_node->setBoolValue(spin >= _gyro_spin_valid_from_node->getDoubleValue());

    // Calculate gyro responsiveness
    double gyro_responsiveness_factor = 1.0 - pow((1.0 - spin), 3);

    // Calculate the indicated turn rate
    const double instrument_yaw_sensitivity = 1.0;   // TODO: make it configurable?
    const double instrument_roll_sensitivity = 0.15; // TODO: make it configurable?

    double yaw_rate = instrument_yaw_sensitivity * _yaw_rate_node->getDoubleValue();
    double roll_rate = instrument_roll_sensitivity * _roll_rate_node->getDoubleValue();

    // Get sign of the dominant vector
    double sign = (fabs(roll_rate) > fabs(yaw_rate) ? roll_rate : yaw_rate);

    // Add vectors (always perpendicular)
    double indicated_turn_rate = sqrt(
        pow(gyro_responsiveness_factor * yaw_rate, 2) +
        pow(gyro_responsiveness_factor * roll_rate, 2));

    indicated_turn_rate = std::copysign(indicated_turn_rate, sign);

    // Clamp the output
    indicated_turn_rate = std::clamp(indicated_turn_rate, -_max_out_degsec, _max_out_degsec);

    // Lag left, based on gyro spin
    indicated_turn_rate = indicated_turn_rate - (1.0 - gyro_responsiveness_factor) * _max_out_degsec;

    // Dampen instrument response
    indicated_turn_rate = fgGetLowPass(_last_rate, indicated_turn_rate, dt * RESPONSIVENESS);

    _last_rate = indicated_turn_rate;
    _rate_out_node->setDoubleValue(indicated_turn_rate);
}

// end of turn_indicator.cxx
