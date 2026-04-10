// FGButtonEvent.hxx -- button and extended-button input event classes
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGInputEvent.hxx"

#include <simgear/structure/SGBinding.hxx>

/// TODO: document
class FGButtonEvent : public FGInputEvent
{
public:
    FGButtonEvent(FGInputDevice* device, SGPropertyNode_ptr node);
    void fire(FGEventData& eventData) override;

    void update(double dt) override;

    enum class OutputMode {
        Button, ///< fire on press, do mod-up binding on release
        Switch  ///< fire with value=true on press, value=false on release
    };

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;

    bool repeatable;
    bool lastState;

    bool _invert = false;
    OutputMode _outputMode = OutputMode::Button;
};

using ButtonEvent_ptr = SGSharedPtr<FGButtonEvent>;

/**
 * FGButtonEvent subclass adding double-press and long-press support.
 *
 * Created by FGInputEvent::NewObject when @c mod-double-press or
 * @c mod-long-press child nodes are present on the event node.
 */
class FGExtendedButtonEvent : public FGButtonEvent
{
public:
    FGExtendedButtonEvent(FGInputDevice* device, SGPropertyNode_ptr node);
    void fire(FGEventData& eventData) override;
    void update(double dt) override;

private:
    void readTimedBindings(SGPropertyNode_ptr eventNode, const char* nodeName, SGBindingList& bindList);
    double readDoubleClickInterval(SGPropertyNode_ptr subNode) const;
    double readLongPressInterval(SGPropertyNode_ptr subNode) const;

    /// Bindings fired on a double-press (second press within _doublePressIntervalSec).
    /// When defined, the second press does not fire the regular down bindings.
    SGBindingList _doublePressBind;

    /// Bindings fired when the button has been held for _longPressIntervalSec.
    /// When defined, the subsequent release does not fire the regular mod-up bindings.
    SGBindingList _longPressBind;

    double _doublePressIntervalSec = 0.0;
    double _longPressIntervalSec = 0.0;

    // double-press detection state
    bool _waitingForDoublePress = false;
    double _timeSinceFirstPress = 0.0;

    // long-press detection state
    double _pressHeldTime = 0.0;
    bool _longPressFired = false;
};
