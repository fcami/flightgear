// FGButtonEvent.cxx -- button and extended-button input event classes
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGButtonEvent.hxx"

#include "FGInputDevice.hxx"
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>

using std::string;

FGButtonEvent::FGButtonEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGInputEvent(device, eventNode),
                                                                                    repeatable(false),
                                                                                    lastState(false)
{
    repeatable = eventNode->getBoolValue("repeatable", repeatable);
    if (eventNode->hasChild("output-mode")) {
        const auto s = eventNode->getStringValue("output-mode");
        if (s == "button") {
            _outputMode = OutputMode::Button;
        } else if (s == "switch") {
            _outputMode = OutputMode::Switch;
        } else {
            throw sg_io_exception("Invalid output mode:" + s, sg_location(eventNode));
        }
    }

    if (_outputMode == OutputMode::Switch) {
        if (repeatable) {
            throw sg_io_exception("Switch mode doesn't support repeatable events", sg_location(eventNode));
        }
    }

    _invert = eventNode->getBoolValue("invert", false);

    if (_outputName.empty()) {
        // this is picked to avoid accidentally overwriting 'value' in
        // existing button bindings
        _outputName = "state";
    }
}

void FGButtonEvent::fire(FGEventData& eventData)
{
    bool pressed = eventData.value > 0.0;
    if (_invert) {
        pressed = !pressed;
    }

    if (_outputMode == OutputMode::Button) {
        // In button mode, we fire the press event on press, and the mod-up binding on release
        if (pressed) {
            // The press event may be repeated.
            if (!lastState || repeatable) {
                SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' has been pressed");
                FGInputEvent::fire(eventData);
            }
        } else {
            // The release event is never repeated.
            if (lastState) {
                SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' has been released");
                eventData.modifiers |= KEYMOD_RELEASED;
                FGInputEvent::fire(eventData);
            }
        }
    } else if (_outputMode == OutputMode::Switch) {
        // In switch mode, we fire with value=true on press, and value=false on release
        SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' has been " << (pressed ? "pressed" : "released"));
        eventData.value = pressed ? 1.0 : 0.0;
        FGInputEvent::fire(eventData);
    }

    lastState = pressed;
}

void FGButtonEvent::update(double dt)
{
    if (repeatable && lastState) {
        // interval / dt handling is done by base ::fire method
        FGEventData ed{1.0, dt, 0 /* modifiers */};
        FGInputEvent::fire(ed);
    }
}

void FGButtonEvent::fire(SGAbstractBinding* binding, FGEventData& eventData)
{
    SGPropertyNode_ptr args(new SGPropertyNode);
    args->setBoolValue(_outputName, eventData.value > 0.0);
    binding->fire(args);
}

///////////////////////////////////////////////////////////////////////////////
// FGExtendedButtonEvent

FGExtendedButtonEvent::FGExtendedButtonEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode)
    : FGButtonEvent(device, eventNode)
{
    readTimedBindings(eventNode, "mod-double-press", _doublePressBind);
    readTimedBindings(eventNode, "mod-long-press", _longPressBind);

    if (auto* n = eventNode->getChild("mod-double-press"))
        _doublePressIntervalSec = readDoubleClickInterval(n);
    if (auto* n = eventNode->getChild("mod-long-press"))
        _longPressIntervalSec = readLongPressInterval(n);
}

void FGExtendedButtonEvent::readTimedBindings(SGPropertyNode_ptr eventNode, const char* nodeName, SGBindingList& bindList)
{
    auto* subNode = eventNode->getChild(nodeName);
    if (!subNode)
        return;
    const auto& nasalMod = device->GetNasalModule();
    for (auto b : subNode->getChildren("binding")) {
        if (b->getStringValue("command") == "nasal" && !nasalMod.empty()) {
            b->setStringValue("module", nasalMod);
        }
        bindList.push_back(SGAbstractBinding::createFromProps(b, globals->get_props()));
    }
}

double FGExtendedButtonEvent::readDoubleClickInterval(SGPropertyNode_ptr subNode) const
{
    return subNode->getDoubleValue("interval-sec",
                                   fgGetDouble("/sim/input/double-press-sec", 0.4));
}

double FGExtendedButtonEvent::readLongPressInterval(SGPropertyNode_ptr subNode) const
{
    return subNode->getDoubleValue("interval-sec",
                                   fgGetDouble("/sim/input/long-press-sec", 0.8));
}

void FGExtendedButtonEvent::fire(FGEventData& eventData)
{
    bool pressed = eventData.value > 0.0;
    if (_invert) {
        pressed = !pressed;
    }

    if (_outputMode == OutputMode::Button) {
        if (pressed) {
            // Reset long-press tracking on each new press
            _pressHeldTime = 0.0;
            _longPressFired = false;

            if (!lastState || repeatable) {
                if (_waitingForDoublePress && !_doublePressBind.empty()) {
                    // Second press within the double-press window: fire double-press bindings
                    // and suppress the normal press bindings
                    SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' double-pressed");
                    for (auto& b : _doublePressBind)
                        FGButtonEvent::fire(b.get(), eventData);
                    _timeSinceFirstPress = 0.0;
                } else {
                    SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' has been pressed");
                    FGInputEvent::fire(eventData);
                    if (!_doublePressBind.empty()) {
                        _waitingForDoublePress = true;
                        _timeSinceFirstPress = 0.0;
                    }
                }
            }
        } else {
            if (lastState) {
                SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' has been released");
                eventData.modifiers |= KEYMOD_RELEASED;
                FGInputEvent::fire(eventData);
                _pressHeldTime = 0.0;
            }
        }
    } else {
        // use regular behaviour for switch / selector
        FGButtonEvent::fire(eventData);
    }

    lastState = pressed;
}

void FGExtendedButtonEvent::update(double dt)
{
    FGButtonEvent::update(dt);

    // Double-press timeout: clear the waiting flag once the window expires
    if (_waitingForDoublePress) {
        _timeSinceFirstPress += dt;
        if (_timeSinceFirstPress >= _doublePressIntervalSec) {
            _waitingForDoublePress = false;
            _timeSinceFirstPress = 0.0;
        }
    }

    // Long-press: fire the long-press bindings once the hold time is exceeded
    if (lastState && !_longPressBind.empty() && !_longPressFired) {
        _pressHeldTime += dt;
        if (_pressHeldTime >= _longPressIntervalSec) {
            SG_LOG(SG_INPUT, SG_DEBUG, "Button '" << this->name << "' long-pressed");
            FGEventData ed{1.0, dt, 0 /* modifiers */};
            for (auto& b : _longPressBind)
                FGButtonEvent::fire(b.get(), ed);
            _longPressFired = true;
        }
    }
}
