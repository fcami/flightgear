// SPDX-FileCopyrightText: (C) 2017 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "test_hidinput.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <simgear/misc/strutils.hxx>
#include <simgear/misc/test_macros.hxx>
#include <simgear/props/props.hxx>

#include <Input/FGHIDDevice.hxx>
#include <Input/FGHIDEventInput.hxx>

// note: these are extracted with usbhid-dump on Linux, or
// mac-hid-dump from https://github.com/todbot/mac-hid-dump

const std::string winWingJoystickDescriptor =
    R"( 05 01 09 04 A1 01 85 01 05 01 09 39 25 07 46 3B
 01 65 14 75 04 95 01 81 42 81 01 65 00 05 09 19
 01 29 80 15 00 25 01 35 00 45 01 75 01 95 80 81
 02 05 01 09 30 15 00 27 FF FF 00 00 35 00 47 FF
 FF 00 00 75 10 95 01 81 02 05 01 09 31 15 00 27
 FF FF 00 00 35 00 47 FF FF 00 00 75 10 95 01 81
 02 05 01 09 32 15 00 27 FF FF 00 00 35 00 47 FF
 FF 00 00 75 10 95 01 81 02 05 01 09 36 15 00 27
 FF 0F 00 00 35 00 47 FF 0F 00 00 75 10 95 01 81
 02 85 02 06 FF 00 09 01 15 00 26 FF 00 35 00 46
 FF 00 75 08 95 0D 81 02 09 02 91 02 C0
)"s;

const std::string tm16000JoystickDescriptor =
    R"(
05  01  09  04  a1  01  85  01  09  30  09  31  15  00  26  ff
3f  75  10  95  02  81  02  09  35  09  36  15  00  26  ff  00
75  08  95  02  81  02  09  39  15  00  25  07  35  00  46  3b
01  65  14  75  04  95  01  81  42  75  04  95  01  81  01  65
00  05  09  19  01  29  11  15  00  25  01  75  01  95  11  81
02  75  07  95  01  81  01  06  00  ff  09  21  75  08  95  35
81  02  06  f0  ff  09  40  85  f2  09  47  75  08  95  3f  b1
02  c0

)"s;

void HIDInputTests::testValueExtract()
{
    uint8_t testDataFromSpec[4] = {0, 0xf4, 0x1 | (0x7 << 2), 0x03};
    CPPUNIT_ASSERT(extractBits(testDataFromSpec, 4, 8, 10) == 500);
    CPPUNIT_ASSERT(extractBits(testDataFromSpec, 4, 18, 10) == 199);

    uint8_t testData2[4] = {0x01 << 6 | 0x0f,
        0x17 | (1 << 6),
        0x3 | (0x11 << 2),
        0x3d | (1 << 6) };

    CPPUNIT_ASSERT(extractBits(testData2, 4, 0, 6) == 15);
    CPPUNIT_ASSERT(extractBits(testData2, 4, 6, 12) == 3421);
    CPPUNIT_ASSERT(extractBits(testData2, 4, 18, 12) == 3921);
    CPPUNIT_ASSERT(extractBits(testData2, 4, 30, 1) == 1);
    CPPUNIT_ASSERT(extractBits(testData2, 4, 31, 1) == 0);
}

// void writeBits(uint8_t* bytes, size_t bitOffset, size_t bitSize, int value)

void HIDInputTests::testValueInsert()
{
    uint8_t buf[8];
    memset(buf, 0, 8);

    int a = 3421;
    int b = 3921;
    writeBits(buf, 6, 12, a);
    writeBits(buf, 18, 12, b);

    CPPUNIT_ASSERT(buf[0] == 0x40);
    CPPUNIT_ASSERT(buf[1] == 0x57);
    CPPUNIT_ASSERT(buf[2] == (0x03 | 0x44));
    CPPUNIT_ASSERT(buf[3] == 0x3d);
}

void HIDInputTests::testSignExtension()
{
    CPPUNIT_ASSERT(signExtend(0x80, 8) == -128);
    CPPUNIT_ASSERT(signExtend(0xff, 8) == -1);
    CPPUNIT_ASSERT(signExtend(0x7f, 8) == 127);

    CPPUNIT_ASSERT(signExtend(0x831, 12) == -1999);
    CPPUNIT_ASSERT(signExtend(0x7dd, 12) == 2013);
}

void HIDInputTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("hid-input");
}

void HIDInputTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------
// testDescriptorParsing
//
// Parse the WinWing joystick USB HID report descriptor and verify that the
// expected items are present with the correct bit offsets, sizes, logical
// ranges and report types.
//
// Descriptor layout (report ID 1):
//   bits  0- 3  : hat switch         (4 bits, abs-hat)
//   bits  4- 7  : padding            (4 bits, undefined-0, constant)
//   bits  8-135 : 128 buttons        (1 bit each)
//   bits 136-151: X axis             (16 bits, abs-x-translate)
//   bits 152-167: Y axis             (16 bits, abs-y-translate)
//   bits 168-183: Z axis             (16 bits, abs-z-translate)
//   bits 184-199: Slider             (16 bits, abs-slider)
// ---------------------------------------------------------------------------
void HIDInputTests::testDescriptorParsing()
{
    const auto descriptorBytes = simgear::strutils::decodeHex(winWingJoystickDescriptor);
    FGHIDDevice dev("test-winwing", descriptorBytes);
    CPPUNIT_ASSERT(dev.parseDescriptorForTesting());

    // --- hat switch ---
    const auto* hat = dev.findItem("abs-hat");
    CPPUNIT_ASSERT_MESSAGE("abs-hat item should exist", hat != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), hat->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), hat->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, hat->logicalMin);
    CPPUNIT_ASSERT_EQUAL(7, hat->logicalMax);
    CPPUNIT_ASSERT(!hat->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-hat"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-hat"));

    // --- synthetic hat axes ---
    const auto* hatX = dev.findItem("abs-hat-x");
    CPPUNIT_ASSERT_MESSAGE("abs-hat-x item should exist", hatX != nullptr);
    CPPUNIT_ASSERT(hatX->isHatX);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), hatX->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), hatX->bitSize);
    CPPUNIT_ASSERT_EQUAL(-1, hatX->logicalMin);
    CPPUNIT_ASSERT_EQUAL(1, hatX->logicalMax);

    const auto* hatY = dev.findItem("abs-hat-y");
    CPPUNIT_ASSERT_MESSAGE("abs-hat-y item should exist", hatY != nullptr);
    CPPUNIT_ASSERT(hatY->isHatY);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), hatY->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), hatY->bitSize);
    CPPUNIT_ASSERT_EQUAL(-1, hatY->logicalMin);
    CPPUNIT_ASSERT_EQUAL(1, hatY->logicalMax);

    // --- padding item (4-bit constant input, no usage) ---
    const auto* padding = dev.findItem("undefined-0");
    CPPUNIT_ASSERT_MESSAGE("undefined-0 (padding) item should exist", padding != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(4), padding->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), padding->bitSize);

    // --- buttons ---
    const auto* btn1 = dev.findItem("button-1");
    CPPUNIT_ASSERT_MESSAGE("button-1 should exist", btn1 != nullptr);
    // buttons start right after hat (4 bits) + padding (4 bits) = bit 8
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(8), btn1->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), btn1->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, btn1->logicalMin);
    CPPUNIT_ASSERT_EQUAL(1, btn1->logicalMax);
    CPPUNIT_ASSERT(!btn1->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("button-1"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("button-1"));

    const auto* btn128 = dev.findItem("button-128");
    CPPUNIT_ASSERT_MESSAGE("button-128 should exist", btn128 != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(135), btn128->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), btn128->bitSize);

    // --- axes (all 16-bit, absolute) ---
    const auto* xAxis = dev.findItem("abs-x-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-x-translate should exist", xAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(136), xAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), xAxis->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, xAxis->logicalMin);
    CPPUNIT_ASSERT_EQUAL(65535, xAxis->logicalMax);
    CPPUNIT_ASSERT(!xAxis->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-x-translate"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-x-translate"));

    const auto* yAxis = dev.findItem("abs-y-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-y-translate should exist", yAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(152), yAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), yAxis->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, yAxis->logicalMin);
    CPPUNIT_ASSERT_EQUAL(65535, yAxis->logicalMax);

    const auto* zAxis = dev.findItem("abs-z-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-z-translate should exist", zAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(168), zAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), zAxis->bitSize);

    const auto* slider = dev.findItem("abs-slider");
    CPPUNIT_ASSERT_MESSAGE("abs-slider should exist", slider != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(184), slider->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), slider->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, slider->logicalMin);
    CPPUNIT_ASSERT_EQUAL(4095, slider->logicalMax);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-slider"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-slider"));
}

// ---------------------------------------------------------------------------
// testConfigureWithDescriptor
//
// Supply the same descriptor via Configure()'s hid-raw-descriptor property,
// then parse and verify the same key items are found with correct properties.
// ---------------------------------------------------------------------------
void HIDInputTests::testConfigureWithDescriptor()
{
    FGHIDDevice dev("configure-test-device");

    SGPropertyNode_ptr node = new SGPropertyNode;
    node->setStringValue("hid-raw-descriptor", winWingJoystickDescriptor);
    dev.Configure(node);

    CPPUNIT_ASSERT(dev.parseDescriptorForTesting());

    // hat switch
    const auto* hat = dev.findItem("abs-hat");
    CPPUNIT_ASSERT_MESSAGE("abs-hat should exist after Configure", hat != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), hat->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), hat->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, hat->logicalMin);
    CPPUNIT_ASSERT_EQUAL(7, hat->logicalMax);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-hat"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-hat"));

    // synthetic hat axes
    CPPUNIT_ASSERT_MESSAGE("abs-hat-x should exist", dev.findItem("abs-hat-x") != nullptr);
    CPPUNIT_ASSERT_MESSAGE("abs-hat-y should exist", dev.findItem("abs-hat-y") != nullptr);

    // buttons
    const auto* btn1 = dev.findItem("button-1");
    CPPUNIT_ASSERT_MESSAGE("button-1 should exist", btn1 != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(8), btn1->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), btn1->bitSize);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("button-1"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("button-1"));

    // axes
    const auto* xAxis = dev.findItem("abs-x-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-x-translate should exist", xAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(136), xAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), xAxis->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, xAxis->logicalMin);
    CPPUNIT_ASSERT_EQUAL(65535, xAxis->logicalMax);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-x-translate"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-x-translate"));

    const auto* slider = dev.findItem("abs-slider");
    CPPUNIT_ASSERT_MESSAGE("abs-slider should exist", slider != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(184), slider->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), slider->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, slider->logicalMin);
    CPPUNIT_ASSERT_EQUAL(4095, slider->logicalMax);
}

// ---------------------------------------------------------------------------
// testTM16000DescriptorParsing
//
// Parse the Thrustmaster T.16000M joystick USB HID report descriptor and
// verify that the expected items are present with the correct properties.
//
// The hidparse library pops usages from the back of its local stack, so when
// multiple usages are declared before a multi-count Input item the order in
// the report is reversed relative to the declaration order.  Concretely:
//
// Report ID 1 layout:
//   bits   0-15  : X axis    (16 bits, abs-x-translate)  <-- declared 1st
//   bits  16-31  : Y axis    (16 bits, abs-y-translate)  <-- declared 2nd
//   bits  32-39  : Rz        ( 8 bits, abs-z-rotate)     <-- declared 1st
//   bits  40-47  : Slider    ( 8 bits, abs-slider)       <-- declared 2nd
//   bits  48-51  : hat       ( 4 bits, abs-hat)
//   bits  52-55  : padding   ( 4 bits, constant)
//   bits  56-72  : buttons 1-17 (1 bit each)
//   bits  73-79  : 7-bit padding (constant)
//   bits  80-503 : 53 vendor bytes
// ---------------------------------------------------------------------------
void HIDInputTests::testTM16000DescriptorParsing()
{
    const auto descriptorBytes = simgear::strutils::decodeHex(tm16000JoystickDescriptor);
    FGHIDDevice dev("test-tm16000", descriptorBytes);
    CPPUNIT_ASSERT(dev.parseDescriptorForTesting());

    // --- axes ---
    const auto* xAxis = dev.findItem("abs-x-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-x-translate should exist", xAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(0), xAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), xAxis->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, xAxis->logicalMin);
    CPPUNIT_ASSERT_EQUAL(16383, xAxis->logicalMax);
    CPPUNIT_ASSERT(!xAxis->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-x-translate"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-x-translate"));

    const auto* yAxis = dev.findItem("abs-y-translate");
    CPPUNIT_ASSERT_MESSAGE("abs-y-translate should exist", yAxis != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(16), yAxis->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(16), yAxis->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, yAxis->logicalMin);
    CPPUNIT_ASSERT_EQUAL(16383, yAxis->logicalMax);
    CPPUNIT_ASSERT(!yAxis->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-y-translate"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-y-translate"));

    const auto* rz = dev.findItem("abs-z-rotate");
    CPPUNIT_ASSERT_MESSAGE("abs-z-rotate should exist", rz != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(32), rz->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(8), rz->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, rz->logicalMin);
    CPPUNIT_ASSERT_EQUAL(255, rz->logicalMax);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-z-rotate"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-z-rotate"));

    const auto* tm16kSlider = dev.findItem("abs-slider");
    CPPUNIT_ASSERT_MESSAGE("abs-slider should exist", tm16kSlider != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(40), tm16kSlider->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(8), tm16kSlider->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, tm16kSlider->logicalMin);
    CPPUNIT_ASSERT_EQUAL(255, tm16kSlider->logicalMax);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-slider"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-slider"));

    // --- hat switch ---
    const auto* hat = dev.findItem("abs-hat");
    CPPUNIT_ASSERT_MESSAGE("abs-hat should exist", hat != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(48), hat->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), hat->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, hat->logicalMin);
    CPPUNIT_ASSERT_EQUAL(7, hat->logicalMax);
    CPPUNIT_ASSERT(!hat->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("abs-hat"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("abs-hat"));

    // --- synthetic hat axes ---
    const auto* tm16kHatX = dev.findItem("abs-hat-x");
    CPPUNIT_ASSERT_MESSAGE("abs-hat-x should exist", tm16kHatX != nullptr);
    CPPUNIT_ASSERT(tm16kHatX->isHatX);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(48), tm16kHatX->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), tm16kHatX->bitSize);

    const auto* tm16kHatY = dev.findItem("abs-hat-y");
    CPPUNIT_ASSERT_MESSAGE("abs-hat-y should exist", tm16kHatY != nullptr);
    CPPUNIT_ASSERT(tm16kHatY->isHatY);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(48), tm16kHatY->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(4), tm16kHatY->bitSize);

    // --- buttons ---
    // Buttons 1-17 follow the 4-bit hat and 4-bit padding at bit 56.
    const auto* tm16kBtn1 = dev.findItem("button-1");
    CPPUNIT_ASSERT_MESSAGE("button-1 should exist", tm16kBtn1 != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(56), tm16kBtn1->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), tm16kBtn1->bitSize);
    CPPUNIT_ASSERT_EQUAL(0, tm16kBtn1->logicalMin);
    CPPUNIT_ASSERT_EQUAL(1, tm16kBtn1->logicalMax);
    CPPUNIT_ASSERT(!tm16kBtn1->isRelative);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("button-1"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("button-1"));

    const auto* tm16kBtn17 = dev.findItem("button-17");
    CPPUNIT_ASSERT_MESSAGE("button-17 should exist", tm16kBtn17 != nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint32_t>(72), tm16kBtn17->bitOffset);
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), tm16kBtn17->bitSize);
    CPPUNIT_ASSERT_EQUAL(HID::ReportType::In, dev.reportTypeForItem("button-17"));
    CPPUNIT_ASSERT_EQUAL(static_cast<uint8_t>(1), dev.reportIdForItem("button-17"));
}
