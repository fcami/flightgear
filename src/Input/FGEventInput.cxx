// FGEventInput.cxx -- handle event driven input devices
//
// Written by Torsten Dreyer, started July 2009.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#include <config.h>

#include "FGEventInput.hxx"

#include <simgear/debug/debug_types.h>
#include <simgear/props/props_io.hxx>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>

using std::map;
using std::string;
using namespace std::string_literals;

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

unsigned FGEventInput::AddDevice(FGInputDevice_ptr inputDevice)
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
            SG_LOG(SG_INPUT, SG_INFO, "using serial-number-specific configuration for device " << nameWithSerial << " : " << configNode->getStringValue("source"));
            inputDevice->SetUniqueName(nameWithSerial);
        }
    }

    if (inputDevice->GetVendorDeviceId() != 0) {
        const auto name = FGDeviceConfigurationMap::nameForVendorDeviceId(inputDevice->GetVendorDeviceId());
        if (configMap.hasConfiguration(name)) {
            configNode = configMap.configurationForDeviceName(name);
            SG_LOG(SG_INPUT, SG_INFO, "using vendor/device-specific configuration for device " << deviceName << " (" << name << ") " << configNode->getStringValue("source"));
            inputDevice->SetUniqueName(deviceName);
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
        return INVALID_DEVICE_INDEX;
    }

    // create this node
    deviceNode = baseNode->getNode("device", index, true);

    // and copy the properties from the configuration tree
    copyProperties(configNode, deviceNode);

    inputDevice->Configure(deviceNode);

    bool ok = inputDevice->Open();
    if (!ok) {
        return INVALID_DEVICE_INDEX;
    }

    inputDevices[deviceNode->getIndex()] = inputDevice;

    // Run Nasal <open> code for the device, now it's open
    inputDevice->postOpen();

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
        inputDevice->doClose();
        inputDevices.erase(index);
    }
    deviceNode = baseNode->removeChild("device", index);
}
