// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

#include <config.h>

#include "test_hidinput.hxx"
#include "test_inputDevice.hxx"


// Set up the unit tests.
#ifdef ENABLE_HID_INPUT
    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HIDInputTests, "Unit tests");
#endif

    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(InputDeviceTests, "Unit tests");
    CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ReportSettingTests, "Unit tests");
