// FGAxisEvent.cxx -- axis input event classes (absolute, relative)
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGAxisEvent.hxx"

#include <simgear/math/SGMath.hxx>
#include <simgear/math/interpolater.hxx>
#include <simgear/structure/exception.hxx>

FGAxisEvent::FGAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGInputEvent(device, eventNode)
{
    tolerance = eventNode->getDoubleValue("tolerance", 0.002);
    minRange = eventNode->getDoubleValue("min-range", 0.0);
    maxRange = eventNode->getDoubleValue("max-range", 0.0);
    center = eventNode->getDoubleValue("center", 0.0);
    deadband = eventNode->getDoubleValue("dead-band", 0.0);

    noiseThresholdBits = eventNode->getIntValue("noise-threshold-bits", 0);

    // interpolation of values
    if (eventNode->hasChild("interpolater")) {
        interpolater.reset(new SGInterpTable{eventNode->getChild("interpolater")});
        mirrorInterpolater = eventNode->getBoolValue("interpolater/mirrored", false);
    }

    if (eventNode->hasChild("output-mode")) {
        const auto s = eventNode->getStringValue("output-mode");
        if (s == "signed-normalized") {
            _outputMode = OutputMode::SignedNormalized;
        } else if (s == "unsigned-normalized") {
            _outputMode = OutputMode::UnsignedNormalized;
        } else if (s == "direct") {
            _outputMode = OutputMode::Direct;
        } else {
            throw sg_io_exception("Invalid output mode:" + s, sg_location(eventNode));
        }
    }

    if (eventNode->hasChild("invert")) {
        _invert = eventNode->getBoolValue("invert", false);
    }

    if (eventNode->hasChild("low")) {
        _lowButton = ButtonEvent_ptr(new FGButtonEvent(device, eventNode->getChild("low")));
    }

    if (eventNode->hasChild("high")) {
        _highButton = ButtonEvent_ptr(new FGButtonEvent(device, eventNode->getChild("high")));
    }

    setDefaultThresholds();
    if (eventNode->hasChild("low-threshold") || eventNode->hasChild("high-threshold")) {
        lowThreshold = eventNode->getDoubleValue("low-threshold", lowThreshold);
        highThreshold = eventNode->getDoubleValue("high-threshold", highThreshold);
    }
}

void FGAxisEvent::setDefaultThresholds()
{
    if (_outputMode == OutputMode::SignedNormalized) {
        lowThreshold = -0.9;
        highThreshold = 0.9;
    } else if (_outputMode == OutputMode::UnsignedNormalized) {
        lowThreshold = 0.1;
        highThreshold = 0.9;
    }
}

void FGAxisEvent::SetDefaultRange(double min, double max)
{
    if ((minRange == 0.0) && (maxRange == 0.0)) {
        minRange = min;
        maxRange = max;
    }
}

FGAxisEvent::~FGAxisEvent() = default;

void FGAxisEvent::update(double dt)
{
    FGInputEvent::update(dt);
    // ensure buttons repeat, since this common for hats
    if (_lowButton) {
        _lowButton->update(dt);
    }
    if (_highButton) {
        _highButton->update(dt);
    }
}

void FGAxisEvent::fire(FGEventData& eventData)
{
    if (fabs(eventData.value - lastValue) < tolerance)
        return;
    lastValue = eventData.value;

    // We need a copy of the  FGEventData struct to set the new value and to avoid side effects
    FGEventData ed = eventData;
    ed.value = computeValue(lastValue);

    if (interpolater) {
        if ((ed.value < 0.0) && mirrorInterpolater) {
            // mirror the positive interpolation for negative values
            ed.value = -interpolater->interpolate(fabs(ed.value));
        } else {
            ed.value = interpolater->interpolate(ed.value);
        }
    }

    FGInputEvent::fire(ed);
    const auto v = ed.value;
    if (_lowButton) {
        ed.value = (v < lowThreshold) ? 1.0 : 0.0;
        _lowButton->fire(ed);
    }

    if (_highButton) {
        ed.value = (v > highThreshold) ? 1.0 : 0.0;
        _highButton->fire(ed);
    }
}

double FGAxisEvent::computeValue(double rawValue) const
{
    const double usedMinRange = minRange;
    const double usedMaxRange = maxRange;

    SG_CLAMP_RANGE(rawValue, usedMinRange, usedMaxRange);
    const auto range = usedMaxRange - usedMinRange;

    double value = rawValue;
    // normalize to -1.0 ... 1.0
    if (_outputMode == OutputMode::SignedNormalized) {
        value = (2.0 * (rawValue - usedMinRange) / range) - 1.0;
        if (_invert) {
            value = -value;
        }

        // apply deadband around center position
        if (fabs(value - center) < deadband) {
            value = center;
        }
    } else if (_outputMode == OutputMode::UnsignedNormalized) {
        value = (rawValue - usedMinRange) / range;
        if (_invert) {
            value = 1.0 - value;
        }
    } else if (_outputMode == OutputMode::Direct) {
        if (_invert) {
            value = maxRange - (rawValue - minRange);
        }
    }

    return value;
}

void FGAxisEvent::fire(SGAbstractBinding* binding, FGEventData& eventData)
{
    SGPropertyNode_ptr args(new SGPropertyNode);
    args->setDoubleValue(_outputName, eventData.value);
    binding->fire(args);
}

FGRelAxisEvent::FGRelAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGAxisEvent(device, eventNode)
{
    // relative axes can't use tolerance
    tolerance = 0.0;
    if (_outputName.empty()) {
        _outputName = "offset";
    }
}

FGAbsAxisEvent::FGAbsAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGAxisEvent(device, eventNode)
{
    if (_outputName.empty()) {
        _outputName = "setting";
    }
}
