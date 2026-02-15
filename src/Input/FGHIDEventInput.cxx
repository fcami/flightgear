// FGHIDEventInput.cxx -- handle event driven input devices via HIDAPI
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 James Turner <james@flightgear.org>

#include "config.h"
#include "simgear/debug/debug_types.h"

#include "FGHIDEventInput.hxx"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string>
#include <unordered_set>

#include <Main/fg_props.hxx>
#include <hidapi/hidapi.h>
#include <hidapi/hidparse.h>

#include <simgear/debug/ErrorReportingCallback.hxx>
#include <simgear/io/lowlevel.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/sg_inlines.h>
#include <simgear/structure/exception.hxx>

#include "FGHIDUsage.hxx"

using simgear::strutils::encodeHex;

namespace HID {

bool shouldPrefixWithAbs(uint32_t usagePage, uint32_t usage)
{
    const auto enumUsage = static_cast<UsagePage>(usagePage);
    if (enumUsage == UsagePage::GenericDesktop) {
        switch (usage) {
        case GD_Wheel:
        case GD_Dial:
        case GD_Hatswitch:
        case GD_Slider:
        case GD_Rx:
        case GD_Ry:
        case GD_Rz:
        case GD_X:
        case GD_Y:
        case GD_Z:
            return true;
        default:
            break;
        }
    }

    return false;
}

ReportType reportTypeFromString(const std::string& s)
{
    if (s == "input") return ReportType::In;
    if (s == "output") return ReportType::Out;
    if (s == "feature") return ReportType::Feature;
    return ReportType::Invalid;
}
} // namespace HID

class FGHIDEventInput::FGHIDEventInputPrivate
{
public:
    FGHIDEventInput* p = nullptr;

    void evaluateDevice(hid_device_info* deviceInfo);
};

// anonymous namespace to define our device subclass
namespace {

class FGHIDDevice : public FGInputDevice
{
public:
    FGHIDDevice(hid_device_info* devInfo,
                FGHIDEventInput* subsys);

    virtual ~FGHIDDevice();

    bool Open() override;
    void Close() override;
    void Configure(SGPropertyNode_ptr node) override;

    void update(double dt) override;
    const char* TranslateEventName(FGEventData& eventData) override;
    void Send(const char* eventName, double value) override;
    void SendFeatureReport(unsigned int reportId, const simgear::UInt8Vector& data) override;
    void SendOutputReport(unsigned int reportId, const simgear::UInt8Vector& data) override;

    class Item
    {
    public:
        Item(const std::string& n, uint32_t offset, uint8_t size) : name(n),
                                                                    bitOffset(offset),
                                                                    bitSize(size)
        {
        }

        std::string name;
        uint32_t bitOffset = 0; // form the start of the report
        uint8_t bitSize = 1;
        bool isRelative = false;
        bool doSignExtend = false;
        int lastValue = 0;
        // int defaultValue = 0;
        // range, units, etc not needed for now
        // hopefully this doesn't need to be a list
        FGInputEvent_ptr event;
    };

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

    void defineReport(SGPropertyNode_ptr reportNode);

    void dumpRawBytes(const std::string& b) const;

    std::vector<Report*> _reports;
    std::string _hidPath;
    hid_device* _device = nullptr;
    bool _haveNumberedReports = false;
    bool _debugRaw = false;

    /// set if we parsed the device description our XML
    /// instead of from the USB data. Useful on Windows where the data
    /// is inaccessible, or devices with broken descriptors
    bool _haveLocalDescriptor = false;

    /// allow specifying the descriptor as hex bytes in XML
    simgear::UInt8Vector _rawXMLDescriptor;

    // all sets which will be send on the next update() call.
    std::set<Report*> _dirtyReports;
};

class HIDEventData : public FGEventData
{
public:
    // item, value, dt, keyModifiers
    HIDEventData(FGHIDDevice::Item* it, int value, double dt, int keyMods) : FGEventData(value, dt, keyMods),
                                                                             item(it)
    {
        assert(item);
    }

    FGHIDDevice::Item* item = nullptr;
};

FGHIDDevice::FGHIDDevice(hid_device_info* devInfo, FGHIDEventInput*)
{
    class_id = "FGHIDDevice";
    _hidPath = devInfo->path;

    std::wstring manufacturerName, productName;
    productName = devInfo->product_string ? std::wstring(devInfo->product_string)
                                          : L"unknown HID device";

    if (devInfo->manufacturer_string) {
        manufacturerName = std::wstring(devInfo->manufacturer_string);
        SetName(simgear::strutils::convertWStringToUtf8(manufacturerName) + " " +
                simgear::strutils::convertWStringToUtf8(productName));
    } else {
        SetName(simgear::strutils::convertWStringToUtf8(productName));
    }

    const auto serial = devInfo->serial_number;
    std::string path(devInfo->path);
    // most devices return an empty serial number, unfortunately
    if ((serial != nullptr) && std::wcslen(serial) > 0) {
        SetSerialNumber(simgear::strutils::convertWStringToUtf8(serial));
    }

    std::string _usage = HID::nameForUsage(devInfo->usage_page, devInfo->usage);
    SG_LOG(SG_INPUT, SG_DEBUG, "HID device " << _hidPath << " " << "0x" << std::hex << devInfo->vendor_id << ":0x" << std::hex << devInfo->product_id << " " << "release " << devInfo->release_number << " " << "usage " << _usage << "(0x" << std::hex << devInfo->usage_page << ":0x" << std::hex << devInfo->usage << ") " << "ifn " << devInfo->interface_number << " " << GetName());
}

FGHIDDevice::~FGHIDDevice()
{
    if (_device) {
        hid_close(_device);
    }
}

void FGHIDDevice::Configure(SGPropertyNode_ptr node)
{
    // base class first
    FGInputDevice::Configure(node);

    if (node->hasChild("hid-descriptor")) {
        _haveLocalDescriptor = true;
        if (debugEvents) {
            SG_LOG(SG_INPUT, SG_INFO, GetUniqueName() << " will configure using local HID descriptor");
        }

        for (auto report : node->getChild("hid-descriptor")->getChildren("report")) {
            defineReport(report);
        }
    }

    if (node->hasChild("hid-raw-descriptor")) {
        _rawXMLDescriptor = simgear::strutils::decodeHex(node->getStringValue("hid-raw-descriptor"));
        if (debugEvents) {
            SG_LOG(SG_INPUT, SG_INFO, GetUniqueName() << " will configure using XML-defined raw HID descriptor");
        }
    }

    if (node->getBoolValue("hid-debug-raw")) {
        _debugRaw = true;
    }
}

bool FGHIDDevice::Open()
{
    SG_LOG(SG_INPUT, SG_INFO, "HID open " << GetUniqueName());
    _device = hid_open_path(_hidPath.c_str());
    if (_device == nullptr) {
        const auto path = SGPath(_hidPath);
        simgear::reportFailure(simgear::LoadFailure::IOError,
                               simgear::ErrorCode::InputDeviceConfig,
                               "Failed to open HID device " + _hidPath + " '" + GetUniqueName() + "'. On Linux you may need to adjust permissions of the device using UDev rules.",
                               path);
        return false;
    }

    if (_rawXMLDescriptor.empty()) {
        _rawXMLDescriptor.resize(2048);
        int descriptorSize = hid_get_report_descriptor(_device, _rawXMLDescriptor.data(), _rawXMLDescriptor.size());
        if (descriptorSize <= 0) {
            SG_LOG(SG_INPUT, SG_WARN, "HID: " << GetUniqueName() << " failed to read HID descriptor");
            return false;
        }

        _rawXMLDescriptor.resize(descriptorSize);
    }

    if (!_haveLocalDescriptor) {
        bool ok = parseUSBHIDDescriptor();
        if (!ok)
            return false;
    }

    for (auto& v : handledEvents) {
        auto reportItem = itemWithName(v.first);
        if (!reportItem.second) {
            SG_LOG(SG_INPUT, SG_WARN, "HID device:" << GetUniqueName() << " has no element for event:" << v.first);
            continue;
        }

        FGInputEvent_ptr event = v.second;
        if (debugEvents) {
            SG_LOG(SG_INPUT, SG_INFO, "\tfound item for event:" << v.first);
        }

        reportItem.second->event = event;
    }

    return true;
}

bool FGHIDDevice::parseUSBHIDDescriptor()
{
#if defined(SG_WINDOWS)
    if (_rawXMLDescriptor.empty()) {
        SG_LOG(SG_INPUT, SG_ALERT, GetUniqueName() << ": on Windows, there is no way to extract the UDB-HID report descriptor. " << "\nPlease supply the report descriptor in the device XML configuration.");
        SG_LOG(SG_INPUT, SG_ALERT, "See this page:<> for information on extracting the report descriptor on Windows");
        return false;
    }
#endif

    if (_debugRaw) {
        SG_LOG(SG_INPUT, SG_INFO, "\nHID: descriptor for:" << GetUniqueName() << "\n\t" << encodeHex(_rawXMLDescriptor, ':'));
    }

    hid_item* rootItem = nullptr;
    hid_parse_reportdesc(_rawXMLDescriptor.data(), _rawXMLDescriptor.size(), &rootItem);
    if (debugEvents) {
        SG_LOG(SG_INPUT, SG_INFO, "\nHID: scan for:" << GetUniqueName());
    }

    parseCollection(rootItem);

    hid_free_reportdesc(rootItem);
    return true;
}

void FGHIDDevice::parseCollection(hid_item* c)
{
    for (hid_item* child = c->collection; child != nullptr; child = child->next) {
        if (child->collection) {
            parseCollection(child);
        } else {
            // leaf item
            parseItem(child);
        }
    }
}

auto FGHIDDevice::getReport(HID::ReportType ty, uint8_t number, bool doCreate) -> Report*
{
    if (number > 0) {
        _haveNumberedReports = true;
    }

    for (auto report : _reports) {
        if ((report->type == ty) && (report->number == number)) {
            return report;
        }
    }

    if (doCreate) {
        auto r = new Report{ty, number};
        _reports.push_back(r);
        return r;
    } else {
        return nullptr;
    }
}

auto FGHIDDevice::itemWithName(const std::string& name) const -> std::pair<Report*, Item*>
{
    for (auto report : _reports) {
        for (auto item : report->items) {
            if (item->name == name) {
                return std::make_pair(report, item);
            }
        }
    }

    return std::make_pair(static_cast<Report*>(nullptr), static_cast<Item*>(nullptr));
}

uint8_t FGHIDDevice::countWithName(const std::string& name) const
{
    uint8_t result = 0;
    size_t nameLength = name.length();

    for (auto report : _reports) {
        for (auto item : report->items) {
            if (strncmp(name.c_str(), item->name.c_str(), nameLength) == 0) {
                result++;
            }
        }
    }

    return result;
}

void FGHIDDevice::parseItem(hid_item* item)
{
    std::string name = HID::nameForUsage(item->usage >> 16, item->usage & 0xffff);
    if (hid_parse_is_relative(item)) {
        name = "rel-" + name; // prefix relative names
    } else if (HID::shouldPrefixWithAbs(item->usage >> 16, item->usage & 0xffff)) {
        name = "abs-" + name;
    }

    const auto ty = static_cast<HID::ReportType>(item->type);
    auto existingItem = itemWithName(name);
    if (existingItem.second) {
        // type fixup
        const HID::ReportType existingItemType = existingItem.first->type;
        if (existingItemType != ty) {
            // might be an item named identically in input/output and feature reports
            //  -> prefix the feature one with 'feature'
            if (ty == HID::ReportType::Feature) {
                name = "feature-" + name;
            } else if (existingItemType == HID::ReportType::Feature) {
                // rename this existing item since it's a feature
                existingItem.second->name = "feature-" + name;
            }
        }
    }

    // do the count now, after we did any renaming, since we might have
    // N > 1 for the new name
    int existingCount = countWithName(name);
    if (existingCount > 0) {
        if (existingCount == 1) {
            // rename existing item 0 to have the "-0" suffix
            auto existingItem = itemWithName(name);
            existingItem.second->name += "-0";
        }

        // define the new nae
        std::stringstream os;
        os << name << "-" << existingCount;
        name = os.str();
    }

    auto report = getReport(ty, item->report_id, true /* create */);
    uint32_t bitOffset = report->currentBitSize();

    if (debugEvents) {
        SG_LOG(SG_INPUT, SG_INFO, GetUniqueName() << ": add:" << name << ", bits: " << bitOffset << ":" << (int)item->report_size << ", report=" << (int)item->report_id);
    }

    Item* itemObject = new Item{name, bitOffset, item->report_size};
    itemObject->isRelative = hid_parse_is_relative(item);
    itemObject->doSignExtend = (item->logical_min < 0) || (item->logical_max < 0);
    report->items.push_back(itemObject);
}

void FGHIDDevice::Close()
{
    if (_device) {
        hid_close(_device);
        _device = nullptr;
    }
}

void FGHIDDevice::update(double dt)
{
    if (!_device) {
        return;
    }

    uint8_t reportBuf[65];
    int readCount = 0;
    while (true) {
        readCount = hid_read_timeout(_device, reportBuf, sizeof(reportBuf), 0);

        if (readCount <= 0) {
            break;
        }

        int modifiers = fgGetKeyModifiers();
        const uint8_t reportNumber = _haveNumberedReports ? reportBuf[0] : 0;
        auto inputReport = getReport(HID::ReportType::In, reportNumber, false);
        if (!inputReport) {
            SG_LOG(SG_INPUT, SG_WARN, GetName() << ": FGHIDDevice: Unknown input report number:" << static_cast<int>(reportNumber));
        } else {
            uint8_t* reportBytes = _haveNumberedReports ? reportBuf + 1 : reportBuf;
            size_t reportSize = _haveNumberedReports ? readCount - 1 : readCount;
            processInputReport(inputReport, reportBytes, reportSize, dt, modifiers);
        }
    }

    FGInputDevice::update(dt);

    for (auto rep : _dirtyReports) {
        sendReport(rep);
    }

    _dirtyReports.clear();
}

void FGHIDDevice::sendReport(Report* report) const
{
    if (!_device) {
        return;
    }

    uint8_t reportBytes[65];
    size_t reportLength = 0;
    memset(reportBytes, 0, sizeof(reportBytes));
    reportBytes[0] = report->number;

    // fill in valid data
    for (auto item : report->items) {
        reportLength += item->bitSize;
        if (item->lastValue == 0) {
            continue;
        }

        writeBits(reportBytes + 1, item->bitOffset, item->bitSize, item->lastValue);
    }

    reportLength /= 8;

    if (_debugRaw) {
        SG_LOG(SG_INPUT, SG_INFO, "sending bytes: " << encodeHex(reportBytes, ':'));
    }

    // send the data, based on the report type
    if (report->type == HID::ReportType::Feature) {
        hid_send_feature_report(_device, reportBytes, reportLength + 1);
    } else {
        assert(report->type == HID::ReportType::Out);
        hid_write(_device, reportBytes, reportLength + 1);
    }
}

int FGHIDDevice::maybeSignExtend(Item* item, int inValue)
{
    return item->doSignExtend ? signExtend(inValue, item->bitSize) : inValue;
}

void FGHIDDevice::processInputReport(Report* report, unsigned char* data,
                                     size_t length,
                                     double dt, int keyModifiers)
{
    if (_debugRaw) {
        SG_LOG(SG_INPUT, SG_INFO, GetName() << " FGHIDDeivce received input report:" << (int)report->number << ", len=" << length);
        {
            SG_LOG(SG_INPUT, SG_INFO, "\tbytes: " << encodeHex(data, length, ':'));
        }
    }

    for (auto item : report->items) {
        int value = extractBits(data, length, item->bitOffset, item->bitSize);

        value = maybeSignExtend(item, value);

        // suppress events for values that aren't changing
        if (item->isRelative) {
            // suppress spurious 0-valued relative events
            if (value == 0) {
                continue;
            }
        } else {
            // suppress no-change events for absolute items
            if (value == item->lastValue) {
                continue;
            }
        }

        item->lastValue = value;
        if (!item->event)
            continue;

        if (_debugRaw) {
            SG_LOG(SG_INPUT, SG_INFO, "\titem:" << item->name << " = " << value);
        }

        HIDEventData event{item, value, dt, keyModifiers};
        HandleEvent(event);
    }
}

void FGHIDDevice::SendFeatureReport(unsigned int reportId, const simgear::UInt8Vector& data)
{
    if (!_device) {
        return;
    }

    if (_debugRaw) {
        SG_LOG(SG_INPUT, SG_INFO, GetName() << ": FGHIDDevice: Sending feature report:" << (int)reportId << ", len=" << data.size());
        SG_LOG(SG_INPUT, SG_INFO, "\tbytes: " << encodeHex(data, ':'));
    }

    uint8_t buf[65];
    size_t len = std::min(data.size() + 1, sizeof(buf));
    buf[0] = reportId;
    memcpy(buf + 1, data.data(), len - 1);
    size_t r = hid_send_feature_report(_device, buf, len);
    if (r < 0) {
        SG_LOG(SG_INPUT, SG_WARN, GetName() << ": FGHIDDevice: Sending feature report failed, error-string is:\n"
                                            << simgear::strutils::error_string(errno));
    }
}

const char* FGHIDDevice::TranslateEventName(FGEventData& eventData)
{
    HIDEventData& hidEvent = static_cast<HIDEventData&>(eventData);
    return hidEvent.item->name.c_str();
}

void FGHIDDevice::Send(const char* eventName, double value)
{
    // even though this called 'Send' it's really 'mark value for sending when we update', to
    // avoid generating multiple output reports in a single update frame.
    auto item = itemWithName(eventName);
    if (item.second == nullptr) {
        SG_LOG(SG_INPUT, SG_WARN, GetName() << ": FGHIDDevice:unknown item name:" << eventName);
        return;
    }

    int intValue = static_cast<int>(value);
    if (item.second->lastValue == intValue) {
        return; // not actually changing
    }

    lastEventName->setStringValue(eventName);
    lastEventValue->setDoubleValue(value);

    // update the stored value prior to sending
    item.second->lastValue = intValue;
    _dirtyReports.insert(item.first);
}

void FGHIDDevice::SendOutputReport(unsigned int reportId, const simgear::UInt8Vector& data)
{
    if (!_device) {
        return;
    }

    auto r = getReport(HID::ReportType::Out, reportId);
    if (!r) {
        SG_LOG(SG_INPUT, SG_DEV_ALERT, "HID device does not define an output report with ID:" << reportId);
        return;
    }


    if (_debugRaw) {
        SG_LOG(SG_INPUT, SG_INFO, GetName() << ": FGHIDDevice: output feature:" << (int)reportId << ", len=" << data.size());
        SG_LOG(SG_INPUT, SG_INFO, "\tbytes: " << encodeHex(data, ':'));
    }

    uint8_t buf[65];
    size_t len = std::min(data.size() + 1, sizeof(buf));
    buf[0] = reportId;
    memcpy(buf + 1, data.data(), len - 1);

    auto result = hid_write(_device, buf, len);
    if (result < 0) {
        SG_LOG(SG_INPUT, SG_DEV_ALERT, GetName() << ": FGHIDDevice: Sending outpit report failed, error-string is:\n"
                                                 << simgear::strutils::error_string(errno));
    }
}

void FGHIDDevice::defineReport(SGPropertyNode_ptr reportNode)
{
    const int nChildren = reportNode->nChildren();
    uint32_t bitCount = 0;
    const auto rty = HID::reportTypeFromString(reportNode->getStringValue("type"));
    if (rty == HID::ReportType::Invalid) {
        SG_LOG(SG_INPUT, SG_WARN, GetName() << ": FGHIDDevice: invalid report type:" << reportNode->getStringValue("type"));
        return;
    }

    const auto id = reportNode->getIntValue("id");
    if (id > 0) {
        _haveNumberedReports = true;
    }

    auto report = new Report(rty, id);
    _reports.push_back(report);

    for (int c = 0; c < nChildren; ++c) {
        const auto n = reportNode->getChild(c);
        const int size = n->getIntValue("size", 1); // default to a single bit
        if (n->getNameString() == "unused-bits") {
            bitCount += size;
            continue;
        }

        if (n->getNameString() == "type" || n->getNameString() == "id") {
            continue; // already handled above
        }

        // allow repeating items
        uint8_t count = n->getIntValue("count", 1);
        std::string name = n->getNameString();
        const auto lastHypen = name.rfind("-");
        std::string baseName = name.substr(0, lastHypen + 1);
        int baseIndex = std::stoi(name.substr(lastHypen + 1));

        const bool isRelative = (name.find("rel-") == 0);
        const bool isSigned = n->getBoolValue("is-signed", false);

        for (uint8_t i = 0; i < count; ++i) {
            std::ostringstream oss;
            oss << baseName << (baseIndex + i);
            Item* itemObject = new Item{oss.str(), bitCount, static_cast<uint8_t>(size)};
            itemObject->isRelative = isRelative;
            itemObject->doSignExtend = isSigned;
            report->items.push_back(itemObject);
            bitCount += size;
        }
    }
}


} // namespace


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

FGHIDEventInput::~FGHIDEventInput()
{
}

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
                std::string _usage = HID::nameForUsage(curDev->usage_page, curDev->usage);
                SG_LOG(SG_INPUT, SG_DEBUG, "Skipping duplicate path " << pathStr << " " << "usage " << _usage << "(0x" << std::hex << curDev->usage_page << ":0x" << std::hex << curDev->usage << ")");
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
    // allocate an input device, and add to the base class to see if we have
    // a config
    p->AddDevice(new FGHIDDevice(deviceInfo, p));
}

///////////////////////////////////////////////////////////////////////////////////////////////
