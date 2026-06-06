// slip_skid_ball.cxx - an slip-skid ball.
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2003 David Megginson (public domain)
// SPDX-FileContributor: Enhanced by Mariusz Matuszek, 2026

#include "config.h"

#include "slip_skid_ball.hxx"
#include <Main/fg_props.hxx>
#include <Main/util.hxx>
#include <algorithm>
#include <cmath>

using std::string;

SlipSkidBall::SlipSkidBall ( SGPropertyNode *node)
    :
    _name(node->getStringValue("name", "slip-skid-ball")),
    _num(node->getIntValue("number", 0)),
    _half_arc_limit_deg(node->getDoubleValue("half-arc-limit-deg", 15.0))
{
}

SlipSkidBall::~SlipSkidBall ()
{
}

void
SlipSkidBall::init ()
{
    string branch;
    branch = "/instrumentation/" + _name;

    SGPropertyNode *node = fgGetNode(branch, _num, true );
    _serviceable_node = node->getChild("serviceable", 0, true);
    _y_accel_node = fgGetNode("/accelerations/pilot/y-accel-fps_sec", true);
    _z_accel_node = fgGetNode("/accelerations/pilot/z-accel-fps_sec", true);
    _out_node_compat = node->getChild("indicated-slip-skid", 0, true);
    _out_node_new = node->getChild("indicated-slip-skid-deg", 0, true);
    _override_node = node->getChild("override", 0, true);

    reinit();
}

void
SlipSkidBall::reinit ()
{
    _out_node_compat->setDoubleValue(0.0);
    _out_node_new->setDoubleValue(0.0);
}

void
SlipSkidBall::update (double delta_time_sec)
{
    double y_accel = _y_accel_node->getDoubleValue();
    double z_accel = _z_accel_node->getDoubleValue();

    if (_serviceable_node->getBoolValue() && !_override_node->getBoolValue()) {

        // Backward-compatible output
        double d = -z_accel;
        if (d < 1.0)
            d = 1.0;
        double pos = y_accel / d * 10.0;

        pos = fgGetLowPass(_out_node_compat->getDoubleValue(), pos, delta_time_sec);
        _out_node_compat->setDoubleValue(pos);

        // Corrected output (both normal and inverted flight, output in degrees)
        if (z_accel > 0.0) {
            // 'negative g' or inverted flight
            pos = std::copysign(_half_arc_limit_deg, y_accel);
        } else {
            // 'normal' flight
            pos = SGMiscd::rad2deg(atan2(y_accel, fabs(z_accel)));
            pos = std::clamp(pos, -_half_arc_limit_deg, _half_arc_limit_deg);
        }

        pos = fgGetLowPass(_out_node_new->getDoubleValue(), pos, 0.16*delta_time_sec); //1Hz low pass
        _out_node_new->setDoubleValue(pos);
    }
}


// Register the subsystem.
#if 0
SGSubsystemMgr::InstancedRegistrant<SlipSkidBall> registrantSlipSkidBall(
    SGSubsystemMgr::FDM,
    {{"instrumentation", SGSubsystemMgr::Dependency::HARD}},
    0.03);
#endif

// end of slip_skid_ball.cxx
