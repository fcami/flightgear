# SPDX-FileCopyrightText: (C) 2025  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_inputDevice.cxx test_inputDevice.hxx
)

fg_add_test_suite(InputDeviceTests u)

if(ENABLE_HID_INPUT)
    target_sources(fgfs_test_suite PRIVATE test_hidinput.cxx test_hidinput.hxx)
    fg_add_test_suite(HIDInputTests u)
endif()
