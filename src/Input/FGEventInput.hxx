// FGEventInput.hxx -- handle event driven input devices
//
// Written by Torsten Dreyer, started July 2009
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2009 Torsten Dreyer

#pragma once

#include "FGCommonInput.hxx"
#include "FGDeviceConfigurationMap.hxx"
#include "FGInputDevice.hxx"

#include <map>

#include <simgear/structure/subsystem_mgr.hxx>

/*
 * The Subsystem for the event input device
 */
/// TODO: document
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

    unsigned AddDevice(FGInputDevice_ptr inputDevice);
    void RemoveDevice(unsigned index);

    std::map<int, FGInputDevice_ptr> inputDevices;
    FGDeviceConfigurationMap configMap;

    SGPropertyNode_ptr nasalClose;

private:
    std::string computeDeviceIndexName(FGInputDevice* dev) const;
};
