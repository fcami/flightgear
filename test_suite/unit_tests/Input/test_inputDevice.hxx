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
    CPPUNIT_TEST(testDoublePress);
    CPPUNIT_TEST(testLongPress);
    CPPUNIT_TEST(testRepeatableWithLongPress);
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
    void testDoublePress();
    void testLongPress();
    void testRepeatableWithLongPress();

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
    TestCommandHandler _doublePressCmd;
    TestCommandHandler _longPressCmd;
};

/**
 * Unit tests for FGReportSetting: nasal code paths and dirty-flag tracking.
 *
 * A separate fixture is needed because FGReportSetting requires FGNasalSys to
 * be present in the subsystem manager, whereas InputDeviceTests deliberately
 * keeps its setUp lightweight.
 */
class ReportSettingTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(ReportSettingTests);
    CPPUNIT_TEST(testNasalInlineCodeString);
    CPPUNIT_TEST(testNasalInlineCodeVector);
    CPPUNIT_TEST(testNasalFunction);
    CPPUNIT_TEST(testWatchDirtyTracking);
    CPPUNIT_TEST(testReportType);
    CPPUNIT_TEST(testNasalCodeArgs);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp() override;
    void tearDown() override;

    void testNasalInlineCodeString();
    void testNasalInlineCodeVector();
    void testNasalInlineCodeNil();
    void testNasalFunction();
    void testWatchDirtyTracking();
    void testReportType();
    void testNasalCodeArgs();
};
