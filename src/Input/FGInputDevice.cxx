// FGInputDevice.cxx -- abstract base class for event-driven input devices
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "FGInputDevice.hxx"

#include <map>
#include <string>
#include <vector>

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/debug/debug_types.h>
#include <simgear/misc/strutils.hxx>
#include <simgear/nasal/cppbind/Ghost.hxx>
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
    if (!deviceNode) {
        return;
    }

    auto debug = deviceNode->getNode("debug-events");
    if (debug) {
        debug->removeChangeListener(_configListener.get());
    }
}

void FGInputDevice::doClose()
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

    // call our virtual method
    Close();
}

static naRef createNasalGhost(FGInputDevice_ptr ref, naContext c)
{
    using NasalInputDevice = nasal::Ghost<FGInputDevice_ptr>;

    // We need a non-const shared pointer for the ghost system
    return NasalInputDevice::makeGhost(c, ref);
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

    auto node = deviceNode->getNode("debug-events", true);
    node->addChangeListener(_configListener.get());

    SGPropertyNode_ptr nasal = deviceNode->getNode("nasal");
    auto nas = globals->get_subsystem<FGNasalSys>();
    const bool haveUpdate = nasal && nasal->hasChild("update");
    const bool haveOpenOrClose = nasal && (nasal->hasChild("open") || nasal->hasChild("close"));
    if (nas && (haveUpdate || haveOpenOrClose)) {
        // pre-create the module hash, so device property is available
        // immediately, eg during <open> code
        naContext c = naNewContext();
        naRef module = nas->getModule(nasalModule, true /*create*/);
        naRef ghost = createNasalGhost(this, c);
        nasal::Hash moduleHash(module, c);
        moduleHash.set("device", ghost);
        naFreeContext(c);
    }

    if (nas && haveUpdate) {
        const auto updateCode = nasal->getChild("update");
        const auto loc = updateCode->getLocation();
        _postUpdateCallback = nas->createCode(updateCode->getStringValue(), loc.getPath(), loc.getLine());
        if (_postUpdateCallback.getErrors().size() > 0) {
            simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Failed to compile update callback for device"s + _postUpdateCallback.getErrors().front(),
                                   sg_location(loc));
            _postUpdateCallback = {};
        }
    }
}

void FGInputDevice::postOpen()
{
    SGPropertyNode_ptr nasal = deviceNode->getNode("nasal");
    auto nas = globals->get_subsystem<FGNasalSys>();

    if (nasal && nas) {
        SGPropertyNode_ptr open = nasal->getNode("open");
        if (open) {
            const string s = open->getStringValue();
            bool ok = nas->createModule(nasalModule.c_str(), nasalModule.c_str(), s.c_str(), s.length(), deviceNode);
            if (!ok) {
                simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                       simgear::ErrorCode::InputDeviceConfig,
                                       "Failed to load device Nasal",
                                       sg_location(open));
            }
        }
    }
}

void FGInputDevice::AddHandledEvent(FGInputEvent_ptr event)
{
    auto it = handledEvents.find(event->GetName());
    if (it == handledEvents.end()) {
        handledEvents.insert(it, std::make_pair(event->GetName(), event));
    }
}

naRef FGInputDevice::getModule()
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    if (!nas) {
        return naRef();
    }

    return nas->getModule(nasalModule, false /*don't create*/);
}

void FGInputDevice::update(double dt)
{
    for (auto it : handledEvents) {
        it.second->update(dt);
    }

    naRef module = naNil();

    bool didSend = false;
    for (auto r : reportSettings) {
        if (r->hasError()) {
            continue;
        }

        try {
            if (r->Test()) {
                if (naIsNil(module)) {
                    module = getModule();
                }

                auto reportData = r->reportBytes(module);
                if (debugEvents) {
                    SG_LOG(SG_INPUT, SG_INFO, class_id << " " << GetUniqueName() << ": Sending report " << r->getReportId() << simgear::strutils::encodeHex(reportData));
                }
                if (r->getReportType() == FGReportSetting::Type::Feature) {
                    SendFeatureReport(r->getReportId(), reportData);
                } else {
                    SendOutputReport(r->getReportId(), reportData);
                }

                didSend = true;
            }
        } catch (sg_exception& e) {
            r->markAsError();
            simgear::reportFailure(simgear::LoadFailure::Unknown,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Failed to send report:"s + e.getMessage(),
                                   e.getLocation());
        }
    } // of report setting iteration

    if (didSend && _postUpdateCallback.isValid()) {
        try {
            if (naIsNil(module)) {
                module = getModule();
            }

            _postUpdateCallback.callWithLocals(module);
        } catch (sg_exception& e) {
            simgear::reportFailure(simgear::LoadFailure::Unknown,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Failed to execute post-update callback:"s + e.getMessage(),
                                   e.getLocation());

            // FIXME
            //_postUpdateCallback.reset();
        }
    }
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
