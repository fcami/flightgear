/*
 * SPDX-FileName: test_Commands.cxx
 * SPDX-FileComment: Unit tests for built-in commands
 * SPDX-FileCopyrightText: Copyright (C) 2023  James Turner
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_commands.hxx"
#include "config.h"

#include <simgear/structure/commands.hxx>

#include "Main/fg_commands.hxx"
#include "Main/fg_props.hxx"
#include "Main/globals.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

using namespace std::string_literals;
using namespace flightgear;

void CommandsTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("commands");
    fgLoadProps("defaults.xml", globals->get_props());

    fgInitCommands();
}

void CommandsTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void CommandsTests::testPropertyAdjustCommand()
{
    auto propAdjust = SGCommandMgr::instance()->getCommand("property-adjust");

    {
        fgSetDouble("/foo", 10.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setDoubleValue("step", 1.0);
        CPPUNIT_ASSERT((*propAdjust)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-adjust step failed", 11.0, fgGetDouble("/foo"), 0.0001);
    }

    {
        fgSetDouble("/foo", 10.0);
        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setDoubleValue("step", 5.0);
        arg->setDoubleValue("max", 12.0);
        CPPUNIT_ASSERT((*propAdjust)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-adjust step with max", 12.0, fgGetDouble("/foo"), 0.0001);
    }

    {
        fgSetDouble("/foo", 30.0);
        fgSetDouble("/wib/bar", 33.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setDoubleValue("step", 5.0);
        arg->setStringValue("max-prop", "/wib/bar");
        CPPUNIT_ASSERT((*propAdjust)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-adjust step with max from prop", 33.0, fgGetDouble("/foo"), 0.0001);
    }

    // check fallback code path if max-prop is missing
    {
        fgSetDouble("/foo", 30.0);
        fgSetDouble("/wib/bar", 33.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setDoubleValue("step", 5.0);
        arg->setStringValue("max-prop", "/wib/xxxbar");
        arg->setDoubleValue("max", 34.0);
        CPPUNIT_ASSERT((*propAdjust)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-adjust step with max from prop", 34.0, fgGetDouble("/foo"), 0.0001);
    }
}

void CommandsTests::testPropertyMultiplyCommand()
{
    auto cmd = SGCommandMgr::instance()->getCommand("property-multiply");

    {
        fgSetDouble("/foo", 10.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setDoubleValue("factor", 4.0);
        CPPUNIT_ASSERT((*cmd)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-multiply failed", 40.0, fgGetDouble("/foo"), 0.0001);
    }

    {
        fgSetDouble("/foo", 10.0);
        fgSetDouble("/bar", 5.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");
        arg->setStringValue("factor-prop", "/bar");
        CPPUNIT_ASSERT((*cmd)(arg, globals->get_props()));

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("property-multiply failed", 50.0, fgGetDouble("/foo"), 0.0001);
    }

    // missing factor
    {
        fgSetDouble("/foo", 10.0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/foo");

        // check the command fails
        CPPUNIT_ASSERT(!(*cmd)(arg, globals->get_props()));
    }
}

void CommandsTests::testPropertyBitCommands()
{
    auto cmdSet = SGCommandMgr::instance()->getCommand("property-set-bit");
    auto cmdClear = SGCommandMgr::instance()->getCommand("property-clear-bit");
    auto cmdToggle = SGCommandMgr::instance()->getCommand("property-toggle-bit");

    // property-set-bit
    {
        fgSetInt("/bits", 0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 3);
        CPPUNIT_ASSERT((*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-set-bit failed", 8, fgGetInt("/bits"));

        // setting a bit that is already set should be idempotent
        CPPUNIT_ASSERT((*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-set-bit idempotent", 8, fgGetInt("/bits"));
    }

    // property-clear-bit
    {
        fgSetInt("/bits", 0xFF);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 4);
        CPPUNIT_ASSERT((*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-clear-bit failed", 0xFF & ~(1 << 4), fgGetInt("/bits"));

        // clearing an already-cleared bit should be idempotent
        CPPUNIT_ASSERT((*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-clear-bit idempotent", 0xFF & ~(1 << 4), fgGetInt("/bits"));
    }

    // property-toggle-bit
    {
        fgSetInt("/bits", 0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 5);
        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit set", 1 << 5, fgGetInt("/bits"));

        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit clear", 0, fgGetInt("/bits"));
    }

    // boundary: bit 0 (LSB)
    {
        fgSetInt("/bits", 0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 0);
        CPPUNIT_ASSERT((*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-set-bit bit 0", 1, fgGetInt("/bits"));

        CPPUNIT_ASSERT((*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-clear-bit bit 0", 0, fgGetInt("/bits"));

        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit bit 0 set", 1, fgGetInt("/bits"));
        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit bit 0 clear", 0, fgGetInt("/bits"));
    }

    // boundary: bit 31 (MSB of a 32-bit int)
    {
        fgSetInt("/bits", 0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 31);
        CPPUNIT_ASSERT((*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-set-bit bit 31", static_cast<int>(1u << 31), fgGetInt("/bits"));

        CPPUNIT_ASSERT((*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-clear-bit bit 31", 0, fgGetInt("/bits"));

        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit bit 31 set", static_cast<int>(1u << 31), fgGetInt("/bits"));
        CPPUNIT_ASSERT((*cmdToggle)(arg, globals->get_props()));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("property-toggle-bit bit 31 clear", 0, fgGetInt("/bits"));
    }

    // reject non-integer property
    {
        fgSetDouble("/floatprop", 1.5);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/floatprop");
        arg->setIntValue("bit", 0);
        CPPUNIT_ASSERT(!(*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT(!(*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT(!(*cmdToggle)(arg, globals->get_props()));
    }

    // reject out-of-range bit index
    {
        fgSetInt("/bits", 0);

        SGPropertyNode_ptr arg(new SGPropertyNode);
        arg->setStringValue("property", "/bits");
        arg->setIntValue("bit", 32);
        CPPUNIT_ASSERT(!(*cmdSet)(arg, globals->get_props()));
        CPPUNIT_ASSERT(!(*cmdClear)(arg, globals->get_props()));
        CPPUNIT_ASSERT(!(*cmdToggle)(arg, globals->get_props()));
    }
}
