// slip_skid_ball.hxx - an slip-skid ball.
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2003 David Megginson (public domain)
// SPDX-FileContributor: Enhanced by Mariusz Matuszek, 2026

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>


/**
 * Model a slip-skid ball.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/half-arc-limit-deg       <- defaults to 15 degrees
 * /accelerations/pilot/y-accel-fps_sec
 * /accelerations/pilot/z-accel-fps_sec
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-slip-skid      <- this is a backward-compatible output
 * /instrumentation/"name"/indicated-slip-skid-deg  <- this is an updated output
 */
class SlipSkidBall : public SGSubsystem
{
public:
    SlipSkidBall ( SGPropertyNode *node );
    virtual ~SlipSkidBall ();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "slip-skid-ball"; }

private:
    std::string _name;
    int _num;
    double _half_arc_limit_deg;

    SGPropertyNode_ptr _serviceable_node;
    SGPropertyNode_ptr _y_accel_node;
    SGPropertyNode_ptr _z_accel_node;
    SGPropertyNode_ptr _out_node_compat; // backwards compatible output
    SGPropertyNode_ptr _out_node_new;
    SGPropertyNode_ptr _override_node;
};
