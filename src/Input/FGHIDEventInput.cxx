// FGHIDEventInput.cxx -- handle event driven input devices via HIDAPI
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 James Turner <james@flightgear.org>

#include "config.h"
#include "simgear/debug/debug_types.h"

#include "FGHIDDevice.hxx"
#include "FGHIDEventInput.hxx"

#include <cstring>
#include <string>
#include <unordered_set>

#include <Main/fg_props.hxx>
#include <hidapi/hidapi.h>

#include <simgear/misc/strutils.hxx>
#include <simgear/structure/exception.hxx>

class FGHIDEventInput::FGHIDEventInputPrivate
{
public:
    FGHIDEventInput* p = nullptr;

    void evaluateDevice(hid_device_info* deviceInfo);
};

int extractBits(uint8_t* bytes, size_t lengthInBytes, size_t bitOffset, size_t bitSize)
{
    const size_t wholeBytesToSkip = bitOffset >> 3;
    const size_t offsetInByte = bitOffset & 0x7;

    // work out how many whole bytes to copy
    const size_t bytesToCopy = std::min(sizeof(uint32_t), (offsetInByte + bitSize + 7) / 8);
    uint32_t v = 0;
    // this goes from byte alignment to word alignment safely
    memcpy((void*)&v, bytes + wholeBytesToSkip, bytesToCopy);

    // shift down so lowest bit is aligned
    v = v >> offsetInByte;

    // mask off any extraneous top bits
    const uint32_t mask = ~(0xffffffff << bitSize);
    v &= mask;

    return v;
}

int signExtend(int inValue, size_t bitSize)
{
    const int m = 1U << (bitSize - 1);
    return (inValue ^ m) - m;
}

void writeBits(uint8_t* bytes, size_t bitOffset, size_t bitSize, int value)
{
    size_t wholeBytesToSkip = bitOffset >> 3;
    uint8_t* dataByte = bytes + wholeBytesToSkip;
    size_t offsetInByte = bitOffset & 0x7;
    size_t bitsInByte = std::min(bitSize, 8 - offsetInByte);
    uint8_t mask = 0xff >> (8 - bitsInByte);

    *dataByte |= ((value & mask) << offsetInByte);

    if (bitsInByte < bitSize) {
        // if we have more bits to write, recurse
        writeBits(bytes, bitOffset + bitsInByte, bitSize - bitsInByte, value >> bitsInByte);
    }
}

FGHIDEventInput::FGHIDEventInput() : FGEventInput("Input/HID", "/input/hid"),
                                     d(new FGHIDEventInputPrivate)
{
    d->p = this; // store back pointer to outer object on pimpl
}

FGHIDEventInput::~FGHIDEventInput() = default;

void FGHIDEventInput::reinit()
{
    SG_LOG(SG_INPUT, SG_INFO, "Re-Initializing HID input bindings");
    FGHIDEventInput::shutdown();
    FGHIDEventInput::init();
    FGHIDEventInput::postinit();
}

void FGHIDEventInput::postinit()
{
    SG_LOG(SG_INPUT, SG_INFO, "HID event input starting up");

    hid_init();

    hid_device_info* devices = hid_enumerate(0 /* vendor ID */, 0 /* product ID */);

    // open each device path only once, it may appear multiple times with different usage value
    std::unordered_set<std::string> seenPaths;
    for (hid_device_info* curDev = devices; curDev != nullptr; curDev = curDev->next) {
        if (curDev->path) {
            std::string pathStr(curDev->path);
            if (seenPaths.find(pathStr) == seenPaths.end()) {
                seenPaths.insert(pathStr);
                d->evaluateDevice(curDev);
            } else {
                SG_BULK_LOG(SG_INPUT, "Skipping duplicate path " << pathStr << " (0x" << std::hex << curDev->usage_page << ":0x" << std::hex << curDev->usage << ")");
            }
        }
    }
    hid_free_enumeration(devices);
}

void FGHIDEventInput::shutdown()
{
    SG_LOG(SG_INPUT, SG_INFO, "HID event input shutting down");
    FGEventInput::shutdown();

    hid_exit();
}

//
// read all elements in each input device
//
void FGHIDEventInput::update(double dt)
{
    FGEventInput::update(dt);
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGHIDEventInput> registrantFGHIDEventInput;

///////////////////////////////////////////////////////////////////////////////////////////////

void FGHIDEventInput::FGHIDEventInputPrivate::evaluateDevice(hid_device_info* deviceInfo)
{
    if (deviceInfo->vendor_id == 0x05ac) {
        // Apple device, definitely not a device we care about
        return;
    }

    // allocate an input device, and add to the base class to see if we have
    // a config
    p->AddDevice(new FGHIDDevice(deviceInfo, p));
}

///////////////////////////////////////////////////////////////////////////////////////////////
