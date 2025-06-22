/*
 * SPDX-FileCopyrightText: (C) 2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_StringInterpolation.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"
#include <Scripting/NasalSys.hxx>


#include "Network/MQTT/StringInterpolation.hxx"

#include <Main/fg_props.hxx>
#include <Main/util.hxx>

extern bool global_nasalMinimalInit;

void StringInterpolationTest::setUp()
{
    FGTestApi::setUp::initTestGlobals("StringInterpolation");
    //FGTestApi::setUp::initNavDataCache();

    fgInitAllowedPaths();
    globals->get_props()->getNode("nasal", true);

    //globals->get_subsystem_mgr()->add<FGInterpolator>();

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();

    globals->get_subsystem_mgr()->add<FGNasalSys>();

    globals->get_subsystem_mgr()->postinit();
}

void StringInterpolationTest::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void StringInterpolationTest::testInterpolation()
{
    fgSetDouble("/test/double-value", 3.14);

    StringInterpolator sut("Hello ${prop:/test/double-value}!");
    auto s = sut.interpolate();
    CPPUNIT_ASSERT_EQUAL(std::string("Hello 3.14!"), s);

    s = StringInterpolator::interpolate("Hello ${prop:/test/double-value}!");
    CPPUNIT_ASSERT_EQUAL(std::string("Hello 3.14!"), s);

    s = StringInterpolator::interpolate("Hello ${nasal:var x = 3.14; return sprintf(\"%.2f\",x); }!");
    CPPUNIT_ASSERT_EQUAL(std::string("Hello 3.14!"), s);

}
