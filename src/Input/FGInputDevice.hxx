// FGInputDevice.hxx -- abstract base class for event-driven input devices
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGInputEvent.hxx"
#include "FGReportSetting.hxx"

#include <cstdint>
#include <map>
#include <string>

#include <simgear/io/lowlevel.hxx>
#include <simgear/props/props.hxx>
#include <simgear/structure/SGReferenced.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

/*
 * A abstract class implementing basic functionality of input devices for
 * all operating systems. This is the base class for the O/S-specific
 * implementation of input device handlers
 */
/// TODO: document
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
