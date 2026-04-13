// FGInputDevice.cxx -- abstract base class for event-driven input devices
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGInputDevice.hxx"

#include <map>
#include <string>

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/debug/debug_types.h>
#include <simgear/misc/strutils.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>
#include <Scripting/NasalSys.hxx>

using std::map;
using std::string;
using namespace std::string_literals;

FGInputDevice::FGInputDevice(std::string aName, std::string aSerial) : name(aName),
                                                                       serialNumber(aSerial),
                                                                       _configListener(std::make_unique<PrivateListener>(this))
{
}

void FGInputDevice::PrivateListener::valueChanged(SGPropertyNode* node)
{
    if (node->getNameString() == "debug-events") {
        device->SetDebugEvents(node->getBoolValue());
    }
}

FGInputDevice::~FGInputDevice()
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    if (nas && deviceNode) {
        SGPropertyNode_ptr nasal = deviceNode->getNode("nasal");
        if (nasal) {
            SGPropertyNode_ptr nasalClose = nasal->getNode("close");
            if (nasalClose) {
                const string s = nasalClose->getStringValue();
                nas->createModule(nasalModule.c_str(), nasalModule.c_str(), s.c_str(), s.length(), deviceNode);
            }
        }
        nas->deleteModule(nasalModule.c_str());
    }

    auto debug = deviceNode->getNode("debug-events");
    if (debug) {
        debug->removeChangeListener(_configListener.get());
    }
}

void FGInputDevice::Configure(SGPropertyNode_ptr aDeviceNode)
{
    deviceNode = aDeviceNode;

    // export our class_id to property tree
    deviceNode->setStringValue("_class-id", class_id);
    deviceNode->setStringValue("serial-number", serialNumber);
    deviceNode->setStringValue("unique-name", _uniqueName);

    SG_LOG(SG_INPUT, SG_DEBUG, "FGInputDevice::Configure");

    // use _uniqueName here so each loaded device gets its own Nasal module
    nasalModule = string("__event:") + _uniqueName;

    for (auto ev : deviceNode->getChildren("event")) {
        try {
            AddHandledEvent(FGInputEvent::NewObject(this, ev));
        } catch (sg_exception& e) {
            simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Event config error: "s + e.getMessage(),
                                   sg_location(ev));
        }
    }

    debugEvents = deviceNode->getBoolValue("debug-events", debugEvents);
    grab = deviceNode->getBoolValue("grab", grab);

    auto reportNodes = deviceNode->getChildren("report");
    for (auto repNode : reportNodes) {
        try {
            FGReportSetting_ptr r = new FGReportSetting(repNode);
            reportSettings.push_back(r);
        } catch (sg_exception& e) {
            simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Event report config error: "s + e.getMessage(),
                                   sg_location(repNode));
        }
    }

    lastEventName = deviceNode->getNode("last-event", true)->getNode("name", true);
    lastEventName->setStringValue("");
    lastEventValue = deviceNode->getNode("last-event")->getNode("value", true);
    lastEventValue->setDoubleValue(0.0);

    SGPropertyNode_ptr nasal = deviceNode->getNode("nasal");
    if (nasal) {
        SGPropertyNode_ptr open = nasal->getNode("open");
        if (open) {
            const string s = open->getStringValue();
            auto nas = globals->get_subsystem<FGNasalSys>();
            if (nas)
                nas->createModule(nasalModule.c_str(), nasalModule.c_str(), s.c_str(), s.length(), deviceNode);
        }
    }

    auto node = deviceNode->getNode("debug-events", true);
    node->addChangeListener(_configListener.get());
}

void FGInputDevice::AddHandledEvent(FGInputEvent_ptr event)
{
    auto it = handledEvents.find(event->GetName());
    if (it == handledEvents.end()) {
        handledEvents.insert(it, std::make_pair(event->GetName(), event));
    }
}

void FGInputDevice::update(double dt)
{
    for (map<string, FGInputEvent_ptr>::iterator it = handledEvents.begin(); it != handledEvents.end(); it++)
        (*it).second->update(dt);

    for (auto r : reportSettings) {
        if (r->hasError()) {
            continue;
        }

        try {
            if (r->Test()) {
                auto reportData = r->reportBytes(nasalModule);
                if (debugEvents) {
                    SG_LOG(SG_INPUT, SG_INFO, class_id << " " << GetUniqueName() << ": Sending report " << r->getReportId() << simgear::strutils::encodeHex(reportData));
                }
                if (r->getReportType() == FGReportSetting::Type::Feature) {
                    SendFeatureReport(r->getReportId(), reportData);
                } else {
                    SendOutputReport(r->getReportId(), reportData);
                }
            }
        } catch (sg_exception& e) {
            r->markAsError();
            simgear::reportFailure(simgear::LoadFailure::Unknown,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Failed to send report:"s + e.getMessage(),
                                   e.getLocation());
        }
    } // of report setting iteration
}

void FGInputDevice::HandleEvent(FGEventData& eventData)
{
    string eventName = TranslateEventName(eventData);
    if (debugEvents) {
        SG_LOG(SG_INPUT, SG_INFO, class_id << " " << GetUniqueName() << " has event " << eventName << " modifiers=" << eventData.modifiers << " value=" << eventData.value);
    }
    lastEventName->setStringValue(eventName);
    lastEventValue->setValue(eventData.value);
    if (handledEvents.count(eventName) > 0) {
        handledEvents[eventName]->fire(eventData);
    }
}

void FGInputDevice::SetName(string name)
{
    this->name = name;
}

void FGInputDevice::SetUniqueName(const std::string& name)
{
    _uniqueName = name;
}

void FGInputDevice::SetSerialNumber(std::string serial)
{
    serialNumber = serial;
}

void FGInputDevice::SendFeatureReport(unsigned int reportId, const simgear::UInt8Vector& data)
{
    SG_LOG(SG_INPUT, SG_WARN, "SendFeatureReport not implemented");
}

void FGInputDevice::SendOutputReport(unsigned int reportId, const simgear::UInt8Vector& data)
{
    SG_LOG(SG_INPUT, SG_WARN, "SendOutputReport not implemented");
}

void FGInputDevice::SetDebugEvents(bool debug)
{
    debugEvents = debug;
}
