// FGInputEvent.cxx -- a configured input event with bindings and settings
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGInputEvent.hxx"
#include "FGAxisEvent.hxx"
#include "FGButtonEvent.hxx"
#include "FGInputDevice.hxx"

#include <simgear/misc/strutils.hxx>

using std::string;

static inline bool StartsWith(string& s, const char* cp)
{
    return s.find(cp) == 0;
}

FGInputEvent* FGInputEvent::NewObject(FGInputDevice* device, SGPropertyNode_ptr eventNode)
{
    string name = eventNode->getStringValue("name", "");
    if (StartsWith(name, "button-")) {
        if (eventNode->hasChild("mod-double-press") || eventNode->hasChild("mod-long-press")) {
            return new FGExtendedButtonEvent(device, eventNode);
        }
        return new FGButtonEvent(device, eventNode);
    }

    if (StartsWith(name, "rel-"))
        return new FGRelAxisEvent(device, eventNode);

    if (StartsWith(name, "abs-"))
        return new FGAbsAxisEvent(device, eventNode);

    return new FGInputEvent(device, eventNode);
}

FGInputEvent::FGInputEvent(FGInputDevice* aDevice, SGPropertyNode_ptr eventNode) : device(aDevice),
                                                                                   lastDt(0.0),
                                                                                   lastSettingValue(std::numeric_limits<float>::quiet_NaN())
{
    name = eventNode->getStringValue("name", "");
    desc = eventNode->getStringValue("desc", "");
    intervalSec = eventNode->getDoubleValue("interval-sec", 0.0);

    read_bindings(eventNode, bindings, KEYMOD_NONE, device->GetNasalModule());

    for (auto child : eventNode->getChildren("setting"))
        settings.push_back(new FGEventSetting(child));
}

FGInputEvent::~FGInputEvent() = default;

// send changed value to device (if condition matches)
void FGInputEvent::update(double dt)
{
    for (auto setting : settings) {
        if (setting->Test()) {
            const double value = setting->GetValue();
            if (value != lastSettingValue) {
                device->Send(GetName(), value);
                lastSettingValue = value;
            }
        }
    }
}

void FGInputEvent::fire(FGEventData& eventData)
{
    lastDt += eventData.dt;
    if (lastDt >= intervalSec) {
        for (auto b : bindings[eventData.modifiers]) {
            fire(b, eventData);
        }

        lastDt -= intervalSec;
    }
}

void FGInputEvent::fire(SGAbstractBinding* binding, FGEventData& eventData)
{
    binding->fire(eventData.value);
}
