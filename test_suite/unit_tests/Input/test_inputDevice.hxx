// SPDX-FileCopyrightText: (C) 2026 James Turner <james@flightgear.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <simgear/props/props.hxx>

/**
 * Helper to capture invocations of a test command registered with SGCommandMgr.
 */
struct TestCommandHandler {
    int callCount = 0;
    double lastSetting = 0.0;
    double lastOffset = 0.0;
    bool lastValue = false;

    void reset()
    {
        callCount = 0;
        lastSetting = 0.0;
        lastOffset = 0.0;
        lastValue = false;
    }

    bool handle(const SGPropertyNode* arg, SGPropertyNode* /*root*/)
    {
        ++callCount;
        lastSetting = arg->getDoubleValue("setting", 0.0);
        lastOffset = arg->getDoubleValue("offset", 0.0);
        lastValue = arg->getBoolValue("value", false);
        return true;
    }
};

/**
 * Unit tests for FGInputDevice and related event classes (FGButtonEvent,
 * FGAbsAxisEvent, FGRelAxisEvent).
 */
class InputDeviceTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(InputDeviceTests);
    CPPUNIT_TEST(testConfigureDevice);
    CPPUNIT_TEST(testSimpleEventBinding);
    CPPUNIT_TEST(testButtonPressRelease);
    CPPUNIT_TEST(testAbsAxisEvent);
    CPPUNIT_TEST(testRelAxisEvent);
    CPPUNIT_TEST(testHighLowThreshold);
    CPPUNIT_TEST(testAxesOutputMode);
    CPPUNIT_TEST(testButtonSwitchMode);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;
    void tearDown() override;

    void testConfigureDevice();
    void testSimpleEventBinding();
    void testButtonPressRelease();
    void testAbsAxisEvent();
    void testRelAxisEvent();
    void testHighLowThreshold();
    void testAxesOutputMode();
    void testButtonSwitchMode();

private:
    TestCommandHandler _simpleCmd;
    TestCommandHandler _buttonPressCmd;
    TestCommandHandler _buttonReleaseCmd;
    TestCommandHandler _axisCmd;
    TestCommandHandler _relCmd;
    TestCommandHandler _lowBtnCmd;
    TestCommandHandler _lowBtnReleaseCmd;
    TestCommandHandler _highBtnCmd;
    TestCommandHandler _highBtnReleaseCmd;
    TestCommandHandler _switchCmd;
};
