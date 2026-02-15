// FGEventInput.cxx -- handle event driven input devices
//
// Written by Torsten Dreyer, started July 2009.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include "simgear/debug/debug_types.h"
#include "simgear/misc/strutils.hxx"
#include "simgear/nasal/nasal.h"
#include "simgear/structure/exception.hxx"

#include <config.h>

#include "FGEventInput.hxx"
#include <Main/fg_props.hxx>
#include <Scripting/NasalSys.hxx>
#include <cstdint>
#include <cstring>
#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/io/sg_file.hxx>
#include <simgear/math/SGMath.hxx>
#include <simgear/math/interpolater.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/props/props_io.hxx>
#include <utility>

using simgear::PropertyList;
using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace std::string_literals;

FGEventSetting::FGEventSetting(SGPropertyNode_ptr base) : value(0.0)
{
    SGPropertyNode_ptr n;

    if ((n = base->getNode("value")) != NULL) {
        valueNode = NULL;
        value = n->getDoubleValue();
    } else {
        n = base->getNode("property");
        if (n == NULL) {
            SG_LOG(SG_INPUT, SG_WARN, "Neither <value> nor <property> defined for event setting:" << base->getLocation());
        } else {
            valueNode = fgGetNode(n->getStringValue(), true);
        }
    }

    if ((n = base->getChild("condition")) != NULL) {
        condition = sgReadCondition(base, n);
    } else {
        simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                               simgear::ErrorCode::InputDeviceConfig,
                               "No condition for event setting",
                               sg_location(base));
    }
}

double FGEventSetting::GetValue()
{
    return valueNode == NULL ? value : valueNode->getDoubleValue();
}

bool FGEventSetting::Test()
{
    return condition == NULL ? true : condition->test();
}

static inline bool StartsWith(string& s, const char* cp)
{
    return s.find(cp) == 0;
}

FGInputEvent* FGInputEvent::NewObject(FGInputDevice* device, SGPropertyNode_ptr eventNode)
{
    string name = eventNode->getStringValue("name", "");
    if (StartsWith(name, "button-"))
        return new FGButtonEvent(device, eventNode);

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

FGInputEvent::~FGInputEvent()
{
}

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
    binding->fire();
}

FGAxisEvent::FGAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGInputEvent(device, eventNode)
{
    tolerance = eventNode->getDoubleValue("tolerance", 0.002);
    minRange = eventNode->getDoubleValue("min-range", 0.0);
    maxRange = eventNode->getDoubleValue("max-range", 0.0);
    center = eventNode->getDoubleValue("center", 0.0);
    deadband = eventNode->getDoubleValue("dead-band", 0.0);
    lowThreshold = eventNode->getDoubleValue("low-threshold", -0.9);
    highThreshold = eventNode->getDoubleValue("high-threshold", 0.9);
    lastValue = 9999999;

    // interpolation of values
    if (eventNode->hasChild("interpolater")) {
        interpolater.reset(new SGInterpTable{eventNode->getChild("interpolater")});
        mirrorInterpolater = eventNode->getBoolValue("interpolater/mirrored", false);
    }
}

FGAxisEvent::~FGAxisEvent() = default;

void FGAxisEvent::fire(FGEventData& eventData)
{
    if (fabs(eventData.value - lastValue) < tolerance)
        return;
    lastValue = eventData.value;

    // We need a copy of the  FGEventData struct to set the new value and to avoid side effects
    FGEventData ed = eventData;

    if (minRange != maxRange)
        ed.value = 2.0 * (eventData.value - minRange) / (maxRange - minRange) - 1.0;

    if (fabs(ed.value) < deadband)
        ed.value = 0.0;

    if (interpolater) {
        if ((ed.value < 0.0) && mirrorInterpolater) {
            // mirror the positive interpolation for negative values
            ed.value = -interpolater->interpolate(fabs(ed.value));
        } else {
            ed.value = interpolater->interpolate(ed.value);
        }
    }

    FGInputEvent::fire(ed);
}

void FGAbsAxisEvent::fire(SGAbstractBinding* binding, FGEventData& eventData)
{
    // sets the "setting" node
    binding->fire(eventData.value);
}

FGRelAxisEvent::FGRelAxisEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGAxisEvent(device, eventNode)
{
    // relative axes can't use tolerance
    tolerance = 0.0;
}

void FGRelAxisEvent::fire(SGAbstractBinding* binding, FGEventData& eventData)
{
    // sets the "offset" node
    binding->fire(eventData.value, 1.0);
}

FGButtonEvent::FGButtonEvent(FGInputDevice* device, SGPropertyNode_ptr eventNode) : FGInputEvent(device, eventNode),
                                                                                    repeatable(false),
                                                                                    lastState(false)
{
    repeatable = eventNode->getBoolValue("repeatable", repeatable);
}

void FGButtonEvent::fire(FGEventData& eventData)
{
    bool pressed = eventData.value > 0.0;
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

FGEventInput::FGEventInput() = default;

FGEventInput::FGEventInput(const char* filePath, const char* propertyRoot) : filePath(filePath),
                                                                             propertyRoot(propertyRoot)
{
}

FGEventInput::~FGEventInput() = default;

void FGEventInput::shutdown()
{
    SG_LOG(SG_INPUT, SG_DEBUG, "FGEventInput::shutdown()");
    auto tmp = inputDevices;
    for (auto it : tmp) {
        RemoveDevice(it.first);
    }
    inputDevices.clear();
}

void FGEventInput::init()
{
    configMap = FGDeviceConfigurationMap(filePath, fgGetNode(propertyRoot, true), "device-named");
}

void FGEventInput::postinit()
{
}

void FGEventInput::update(double dt)
{
    for (auto it : inputDevices) {
        it.second->update(dt);
    }
}

std::string FGEventInput::computeDeviceIndexName(FGInputDevice* dev) const
{
    int count = 0;
    const auto devName = dev->GetName();
    for (auto it : inputDevices) {
        if (it.second->GetName() == devName) {
            ++count;
        }
    }

    std::ostringstream os;
    os << devName << "_" << count;
    return os.str();
}

unsigned FGEventInput::AddDevice(FGInputDevice* inputDevice)
{
    SGPropertyNode_ptr baseNode = fgGetNode(propertyRoot, true);
    SGPropertyNode_ptr deviceNode = nullptr;

    const string deviceName = inputDevice->GetName();
    SGPropertyNode_ptr configNode = nullptr;

    // if we have a serial number set, try using that to select a specific configuration
    if (!inputDevice->GetSerialNumber().empty()) {
        const string nameWithSerial = deviceName + "::" + inputDevice->GetSerialNumber();
        if (configMap.hasConfiguration(nameWithSerial)) {
            configNode = configMap.configurationForDeviceName(nameWithSerial);
            SG_LOG(SG_INPUT, SG_INFO, "using instance-specific configuration for device " << nameWithSerial << " : " << configNode->getStringValue("source"));
            inputDevice->SetUniqueName(nameWithSerial);
        }
    }
    if (configNode == nullptr) {
        const auto nameWithIndex = computeDeviceIndexName(inputDevice);
        // try instanced (counted) name
        if (configMap.hasConfiguration(nameWithIndex)) {
            configNode = configMap.configurationForDeviceName(nameWithIndex);
            SG_LOG(SG_INPUT, SG_INFO, "using instance-specific configuration for device " << nameWithIndex << " : " << configNode->getStringValue("source"));
        }
        // otherwise try the unmodified name for the device
        else if (configMap.hasConfiguration(deviceName)) {
            configNode = configMap.configurationForDeviceName(deviceName);
        } else {
            SG_LOG(SG_INPUT, SG_INFO, "No configuration found for device " << deviceName);
            delete inputDevice;
            return INVALID_DEVICE_INDEX;
        }
        inputDevice->SetUniqueName(nameWithIndex);
    }

    // found - copy to /input/event/device[n]
    // find a free index
    unsigned int index;
    for (index = 0; index < MAX_DEVICES; index++) {
        if ((deviceNode = baseNode->getNode("device", index, false)) == nullptr)
            break;
    }

    if (index == MAX_DEVICES) {
        SG_LOG(SG_INPUT, SG_WARN, "To many event devices - ignoring " << inputDevice->GetUniqueName());
        delete inputDevice;
        return INVALID_DEVICE_INDEX;
    }

    // create this node
    deviceNode = baseNode->getNode("device", index, true);

    // and copy the properties from the configuration tree
    copyProperties(configNode, deviceNode);

    inputDevice->Configure(deviceNode);

    bool ok = inputDevice->Open();
    if (!ok) {
        // TODO repot a better error here, to the user
        SG_LOG(SG_INPUT, SG_ALERT, "can't open InputDevice " << inputDevice->GetUniqueName());
        delete inputDevice;
        return INVALID_DEVICE_INDEX;
    }

    inputDevices[deviceNode->getIndex()] = inputDevice;

    SG_LOG(SG_INPUT, SG_INFO, inputDevice->class_id << "::AddDevice '" << inputDevice->GetUniqueName() << "' s/n: " << inputDevice->GetSerialNumber());
    return deviceNode->getIndex();
}

void FGEventInput::RemoveDevice(unsigned index)
{
    // not fully implemented yet
    SGPropertyNode_ptr baseNode = fgGetNode(propertyRoot, true);
    SGPropertyNode_ptr deviceNode = NULL;

    SG_LOG(SG_INPUT, SG_DEBUG, "FGEventInput::RemoveDevice(" << index << ") ");
    FGInputDevice* inputDevice = inputDevices[index];
    if (inputDevice) {
        SG_LOG(SG_INPUT, SG_DEBUG, "\tremoving (" << index << ") " << inputDevice->GetUniqueName());
        inputDevice->Close();
        inputDevices.erase(index);
        delete inputDevice;
    }
    deviceNode = baseNode->removeChild("device", index);
}

FGReportSetting::FGReportSetting(SGPropertyNode_ptr base)
{
    location = base->getLocation();
    reportId = base->getIntValue("report-id");
    nasalFunction = base->getStringValue("nasal-function");

    if (base->hasChild("report-type")) {
        const auto s = base->getStringValue("report-type");
        if (s == "output") {
            _type = Type::Output;
        } else if (s == "feature") {
            _type = Type::Feature;
        } else {
            simgear::reportFailure(simgear::LoadFailure::Misconfigured,
                                   simgear::ErrorCode::InputDeviceConfig,
                                   "Invalid report type:" + s,
                                   sg_location(base));
        }
    }

    auto watchNodes = base->getChildren("watch");
    for (auto w : watchNodes) {
        std::string path = w->getStringValue();
        SGPropertyNode_ptr n = globals->get_props()->getNode(path, true);
        n->addChangeListener(this);
    }
}

bool FGReportSetting::Test()
{
    bool d = dirty;
    dirty = false;
    return d;
}

simgear::UInt8Vector FGReportSetting::reportBytes(const std::string& moduleName) const
{
    auto nas = globals->get_subsystem<FGNasalSys>();
    if (!nas) {
        return {};
    }

    naRef module = nas->getModule(moduleName.c_str());
    if (naIsNil(module)) {
        throw sg_exception("Unknown Nasal module:" + moduleName, nasalFunction, sg_location(location));
    }

    naRef func = naHash_cget(module, (char*)nasalFunction.c_str());
    if (!naIsFunc(func)) {
        throw sg_exception("Not a Nasal function:" + nasalFunction, nasalFunction, sg_location(location));
    }

    naRef result = nas->call(func, 0, 0, naNil());
    if (naIsString(result)) {
        size_t len = naStr_len(result);
        char* bytes = naStr_data(result);
        char* endByte = bytes + len;
        return simgear::UInt8Vector(
            reinterpret_cast<uint8_t*>(bytes),
            reinterpret_cast<uint8_t*>(endByte));
    }

    if (naIsVector(result)) {
        int len = naVec_size(result);
        simgear::UInt8Vector d;
        for (int b = 0; b < len; ++b) {
            int num = naNumValue(naVec_get(result, b)).num;
            d.push_back(static_cast<uint8_t>(num));
        }

        return d;
    }

    // allow returning nil to mean no data
    if (naIsNil(result)) {
        return {};
    }

    throw sg_exception("Bad data from report setting", nasalFunction,
                       sg_location(location));
}

void FGReportSetting::valueChanged(SGPropertyNode* n)
{
    auto it = watchValueCache.find(n);
    const auto val = n->getStringValue();
    if (it == watchValueCache.end()) {
        watchValueCache.insert(std::make_pair(n, val));
        dirty = true;
        return;
    }

    // because we use string equality, for floating-point values, we will
    // quantise to the precision of the string representation.
    // that's an almost-feature, until we define explicit precision on properties
    if (val == it->second) {
        return;
    }

    it->second = val;
    dirty = true;
}
