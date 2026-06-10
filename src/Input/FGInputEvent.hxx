// FGInputEvent.hxx -- a configured input event with bindings and settings
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGCommonInput.hxx"
#include "FGReportSetting.hxx"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <Main/fg_os.hxx> // for KEYMOD_MAX
#include <simgear/props/props.hxx>
#include <simgear/structure/SGReferenced.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

// forward decls
class FGInputDevice;

/// TODO: document
struct FGEventData {
    FGEventData(double aValue, double aDt, int aModifiers) : modifiers(aModifiers), value(aValue), dt(aDt) {}
    int modifiers{0};
    double value{0.0};
    double dt{0.0};
};

/*
 * A wrapper class for a configured event.
 *
 * <event>
 *   <desc>Change the view pitch</desc>
 *   <name>rel-x-rotate</name>
 *   <binding>
 *     <command>property-adjust</command>
 *     <property>sim/current-view/pitch-offset-deg</property>
 *     <factor type="double">0.01</factor>
 *     <min type="double">-90.0</min>
 *     <max type="double">90.0</max>
 *     <wrap type="bool">false</wrap>
 *   </binding>
 *   <mod-xyz>
 *    <binding>
 *      ...
 *    </binding>
 *   </mod-xyz>
 * </event>
 */
/// TODO: document
class FGInputEvent : public SGReferenced,
                     FGCommonInput
{
public:
    FGInputEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode);
    virtual ~FGInputEvent();

    // dispatch the event value through all bindings
    virtual void fire(FGEventData& eventData);

    std::string GetName() const { return name; }
    std::string GetDescription() const { return desc; }

    virtual void update(double dt);
    static FGInputEvent* NewObject(FGInputDevice* device, SGPropertyNode_ptr node);

protected:
    virtual void fire(SGAbstractBinding* binding, FGEventData& eventData);
    /* A more or less meaningful description of the event */
    std::string desc;

    /* One of the predefined names of the event */
    std::string name;

    /* A list of SGBinding objects */
    binding_list_t bindings[KEYMOD_MAX];

    /* A list of FGEventSetting objects */
    setting_list_t settings;

    /* A pointer to the associated device */
    FGInputDevice* device;

    double lastDt = std::nan("");
    double intervalSec = std::nan("");
    double lastSettingValue = std::nan("");

    /**
     * Name of the value to pass when firing bindings. Defaults to 'setting' for
     * axes and 'value' for buttons, for compatibility with property-scale and
     * property-assign commands.
     */
    std::string _outputName;
};

typedef class SGSharedPtr<FGInputEvent> FGInputEvent_ptr;
