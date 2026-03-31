// SPDX-FileCopyrightText: 2012 Ian Dall <ian@beware.dropbear.id.au>
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileComment: Improved by ThunderFly s.r.o. <info@thunderfly.cz>

#pragma once

#include "Engine.hpp"

namespace yasim {

class ElectricEngine : public Engine {
public:
    virtual ElectricEngine* isElectricEngine() { return this; }

    // Initializes an engine from known "takeoff" parameters.
    // voltage - power supply voltage applied to the motor in volts
    // Kv - electric motor velocity constant in RPM/1V
    // Rm - engine winding resistance in Ohms
    ElectricEngine(float voltage, float Kv, float Rm);

    virtual void calc(float pressure, float temp, float speed);
    virtual void stabilize() { return; };
    virtual void integrate(float dt) { return; };
    virtual float getTorque() { return _torque; };
    virtual float getFuelFlow() { return 0; };
    virtual float getOmega0() { return _omega0; };

private:
    // Static configuration:
    float _omega0 {0};		// Reference engine speed
    float _minThrottle {0.05f};		// minimum throttle [0:1]
    float _K {0};			// _K = Eb/omega = tau/Ia
    float _Rm{1};           // engine windig resistance

    // Runtime state/output:
    float _torque {0};  // actual torque of the motor
    float _torque0 {1};   // nominal operating torque
};

}; // namespace yasim
