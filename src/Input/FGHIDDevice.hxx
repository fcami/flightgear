// FGHIDDevice.hxx -- HID input device implementation via HIDAPI
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 James Turner <james@flightgear.org>

#pragma once

#include "FGHIDUsage.hxx"
#include "FGInputDevice.hxx"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <hidapi/hidapi.h>
#include <hidapi/hidparse.h>

class FGHIDEventInput;

/// TODO: document
class FGHIDDevice : public FGInputDevice
{
public:
    FGHIDDevice(hid_device_info* devInfo,
                FGHIDEventInput* subsys);

    /// Test-only constructor: creates a device with a known raw HID descriptor
    /// without requiring access to physical hardware. If rawDescriptor is empty,
    /// the descriptor must be supplied later via Configure() before calling
    /// parseDescriptorForTesting().
    explicit FGHIDDevice(const std::string& name,
                         const simgear::UInt8Vector& rawDescriptor = {});

    virtual ~FGHIDDevice();

    bool Open() override;
    void Close() override;
    void Configure(SGPropertyNode_ptr node) override;

    void update(double dt) override;
    const char* TranslateEventName(FGEventData& eventData) override;
    void Send(const char* eventName, double value) override;
    void SendFeatureReport(unsigned int reportId, const simgear::UInt8Vector& data) override;
    void SendOutputReport(unsigned int reportId, const simgear::UInt8Vector& data) override;

    /// TODO: document
    class Item
    {
    public:
        Item(const std::string& n, uint32_t offset, uint8_t size) : name(n),
                                                                    bitOffset(offset),
                                                                    bitSize(size)
        {
        }

        std::string name;
        uint32_t bitOffset = 0; // from the start of the report
        uint8_t bitSize = 1;
        bool isRelative = false;
        bool doSignExtend = false;
        int lastValue = 0;
        int logicalMin = 0, logicalMax = 0;
        FGInputEvent_ptr event;
        bool isHatX = false;
        bool isHatY = false;
        bool zeroIsCenter = false; // is 0 the center, or North?
    };

    /// Parse the raw descriptor that was either supplied at construction time or
    /// stored by Configure() via hid-raw-descriptor.  Intended for use in unit
    /// tests where Open() cannot be called because there is no real hardware.
    /// Returns true on success.
    bool parseDescriptorForTesting();

    /// Return a const pointer to the named item across all reports, or nullptr.
    const Item* findItem(const std::string& name) const;

    /// Return the report type (In / Out / Feature) of the named item, or Invalid.
    HID::ReportType reportTypeForItem(const std::string& name) const;

    /// Return the report ID of the named item, or 0 if not found.
    uint8_t reportIdForItem(const std::string& name) const;

private:
    class Report
    {
    public:
        Report(HID::ReportType ty, uint8_t n = 0) : type(ty), number(n) {}

        HID::ReportType type;
        uint8_t number = 0;
        std::vector<Item*> items;

        uint32_t currentBitSize() const
        {
            uint32_t size = 0;
            for (auto i : items) {
                if (i->isHatX || i->isHatY) {
                    // these are virtual items, they don't take up bits in the report
                    continue;
                }

                size += i->bitSize;
            }
            return size;
        }
    };

    bool parseUSBHIDDescriptor();
    void parseCollection(hid_item* collection);
    void parseItem(hid_item* item);

    Report* getReport(HID::ReportType ty, uint8_t number, bool doCreate = false);

    void sendReport(Report* report) const;

    uint8_t countWithName(const std::string& name) const;
    std::pair<Report*, Item*> itemWithName(const std::string& name) const;

    void processInputReport(Report* report, unsigned char* data, size_t length,
                            double dt, int keyModifiers);

    int maybeSignExtend(Item* item, int inValue);
    int adjustHatValue(Item* item, int inValue);

    void generateHatEvents(Item* item, int value, double dt, int keyModifiers);

    void defineReport(SGPropertyNode_ptr reportNode);

    void dumpRawBytes(const std::string& b) const;

    std::vector<Report*> _reports;
    std::string _hidPath;
    hid_device* _device = nullptr;
    bool _haveNumberedReports = false;
    bool _debugRaw = false;

    /// set if we parsed the device description from our XML
    /// instead of from the USB data. Useful on Windows where the data
    /// is inaccessible, or devices with broken descriptors
    bool _haveLocalDescriptor = false;

    /// allow specifying the descriptor as hex bytes in XML
    simgear::UInt8Vector _rawXMLDescriptor;

    // all sets which will be send on the next update() call.
    std::set<Report*> _dirtyReports;
};
