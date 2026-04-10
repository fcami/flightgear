// FGEventInput.hxx -- handle event driven input devices
//
// Written by Torsten Dreyer, started July 2009
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGCommonInput.hxx"

#include <cstdint>
#include <memory>
#include <vector>

#include "FGButton.hxx"
#include "FGDeviceConfigurationMap.hxx"
#include "simgear/structure/SGSourceLocation.hxx"
#include <simgear/misc/strutils.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "FGEventInput_private.hxx"

// forward decls
class SGInterpTable;

/*
 * A base structure for event data.
 * To be extended for O/S specific implementation data
 */
struct FGEventData {
    FGEventData(double aValue, double aDt, int aModifiers) : modifiers(aModifiers), value(aValue), dt(aDt) {}
    int modifiers{0};
    double value{0.0};
    double dt{0.0};
};

class FGButtonEvent;
using ButtonEvent_ptr = SGSharedPtr<FGButtonEvent>;

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
class FGInputDevice;
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
};

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

class FGRelAxisEvent : public FGAxisEvent
{
public:
    FGRelAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode);

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};

class FGAbsAxisEvent : public FGAxisEvent
{
public:
    FGAbsAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGAxisEvent(device, eventNode) {}

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};

typedef class SGSharedPtr<FGInputEvent> FGInputEvent_ptr;

/*
 * A abstract class implementing basic functionality of input devices for
 * all operating systems. This is the base class for the O/S-specific
 * implementation of input device handlers
 */
class FGInputDevice : public SGReferenced
{
public:
    FGInputDevice() {}
    FGInputDevice(std::string aName, std::string aSerial = {}) : name(aName), serialNumber(aSerial) {}

    virtual ~FGInputDevice();

    virtual bool Open() = 0;
    virtual void Close() = 0;

    virtual void Send(const char* eventName, double value) = 0;

    inline void Send(const std::string& eventName, double value)
    {
        Send(eventName.c_str(), value);
    }

    virtual void SendFeatureReport(unsigned int reportId, const simgear::UInt8Vector& data);
    virtual void SendOutputReport(unsigned int reportId, const simgear::UInt8Vector& data);

    virtual const char* TranslateEventName(FGEventData& eventData) = 0;


    void SetName(std::string name);
    std::string& GetName() { return name; }

    void SetUniqueName(const std::string& name);
    const std::string GetUniqueName() const { return _uniqueName; }

    void SetSerialNumber(std::string serial);
    std::string& GetSerialNumber() { return serialNumber; }

    void HandleEvent(FGEventData& eventData);

    virtual void AddHandledEvent(FGInputEvent_ptr handledEvent);

    virtual void Configure(SGPropertyNode_ptr deviceNode);

    virtual void update(double dt);

    bool GetDebugEvents() const { return debugEvents; }

    bool GetGrab() const { return grab; }

    const std::string& GetNasalModule() const { return nasalModule; }
    std::string class_id = "FGInputDevice";

    // allow matching based on HID IDs, as well as names
    uint32_t GetVendorDeviceId() const
    {
        return _vendorDeviceId;
    }

    void SetVendorDeviceId(uint32_t id)
    {
        _vendorDeviceId = id;
    }

protected:
    // A map of events, this device handles
    std::map<std::string, FGInputEvent_ptr> handledEvents;

    // the device has a name to be recognized
    std::string name;

    // serial number string to disambiguate multiple instances
    // of the same device
    std::string serialNumber;

    // print out events coming in from the device
    // if true
    bool debugEvents = false;

    // grab the device exclusively, if O/S supports this
    // so events are not sent to other applications
    bool grab = false;

    //configuration in property tree
    SGPropertyNode_ptr deviceNode;
    SGPropertyNode_ptr lastEventName;
    SGPropertyNode_ptr lastEventValue;

    std::string nasalModule;

    report_setting_list_t reportSettings;

    /// name, but with suffix / serial appended. This is important
    /// when loading the device multiple times, to ensure the Nasal
    /// module is unique
    std::string _uniqueName;

    uint32_t _vendorDeviceId = 0;
};

typedef SGSharedPtr<FGInputDevice> FGInputDevice_ptr;


/*
 * The Subsystem for the event input device
 */
class FGEventInput : public SGSubsystem,
                     FGCommonInput
{
public:
    FGEventInput();
    FGEventInput(const char* filePath, const char* propertyRoot);
    virtual ~FGEventInput();

    // Subsystem API.
    void init() override;
    void postinit() override;
    void shutdown() override;
    void update(double dt) override;

    const static unsigned MAX_DEVICES = 1000;
    const static unsigned INVALID_DEVICE_INDEX = MAX_DEVICES + 1;

protected:
    // where to search for configs and where to put them in the property tree
    const char* filePath;
    const char* propertyRoot;

    unsigned AddDevice(FGInputDevice* inputDevice);
    void RemoveDevice(unsigned index);

    std::map<int, FGInputDevice*> inputDevices;
    FGDeviceConfigurationMap configMap;

    SGPropertyNode_ptr nasalClose;

private:
    std::string computeDeviceIndexName(FGInputDevice* dev) const;
};
