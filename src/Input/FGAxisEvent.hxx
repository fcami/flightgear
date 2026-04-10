// FGAxisEvent.hxx -- axis input event classes (absolute, relative)
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGButtonEvent.hxx"
#include "FGInputEvent.hxx"

#include <limits>
#include <memory>

class SGInterpTable;

/// TODO: document
class FGAxisEvent : public FGInputEvent
{
public:
    FGAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode);
    ~FGAxisEvent();

    void update(double dt) override;

    void SetRange(double min, double max)
    {
        minRange = min;
        maxRange = max;
    }

    /**
     * @brief set the range based on system data (eg, HID descriptor logical range)
     * only used if the config node didn't define range data
     */
    void SetDefaultRange(double min, double max);

    enum class OutputMode {
        SignedNormalized,   ///< output in range [-1.0, 1.0], with center at 0.0
        UnsignedNormalized, ///< output in range [0.0, 1.0],
        Direct
    };

protected:
    void fire(FGEventData& eventData) override;

    double computeValue(double rawValue) const;
    void setDefaultThresholds();

    double tolerance = 0.0;
    double minRange = 0.0;
    double maxRange = 0.0;
    double center = 0.0;
    double deadband = 0.0;
    double lowThreshold = 0.0;
    double highThreshold = 0.0;
    double lastValue = std::numeric_limits<double>::quiet_NaN();

    std::unique_ptr<SGInterpTable> interpolater;
    bool mirrorInterpolater = false;

    bool _invert = false;
    OutputMode _outputMode = OutputMode::SignedNormalized;

    ButtonEvent_ptr _lowButton, _highButton;
};

/// TODO: document
class FGRelAxisEvent : public FGAxisEvent
{
public:
    FGRelAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode);

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};

/// TODO: document
class FGAbsAxisEvent : public FGAxisEvent
{
public:
    FGAbsAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGAxisEvent(device, eventNode) {}

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};
