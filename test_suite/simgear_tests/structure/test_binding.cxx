/*
SPDX-FileCopyrightText: 2026 James Turner
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <algorithm>
#include <memory>

#include "test_binding.hxx"

#include "cppunit/TestAssert.h"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include "Main/globals.hxx"
#include "simgear/props/props.hxx"
#include "simgear/props/props_io.hxx"
#include "simgear/structure/SGBinding.hxx"
#include <simgear/structure/commands.hxx>

using namespace std;

namespace {


SGPropertyNode_ptr configFromString(const std::string& s)
{
    SGPropertyNode_ptr config = new SGPropertyNode;
    std::istringstream iss(s);
    readProperties(iss, config);
    return config;
}

} // namespace

// Set up function for each test.
void SimgearBindingTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("Bindings");


    SGCommandMgr::instance()->addCommand("property-toggle", [](const SGPropertyNode* arg, SGPropertyNode* root) {
        // each time the command is run, increment this counter
        globals->get_props()->setIntValue("toggle-count", globals->get_props()->getIntValue("toggle-count") + 1);
        return true;
    });

    SGCommandMgr::instance()->addCommand("property-assign", [](const SGPropertyNode* arg, SGPropertyNode* root) {
        globals->get_props()->setIntValue("assign-count", globals->get_props()->getIntValue("assign-count") + 1);
        return true;
    });
}

// Clean up after each test.
void SimgearBindingTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void SimgearBindingTests::testRegularBinding()
{
    auto config = configFromString(R"(<?xml version="1.0" encoding="UTF-8"?>
        <PropertyList>
        <mybind>
            <command>property-toggle</command>
            <property>/foo</property>
        </mybind>
        <mybind>
            <command>property-assign</command>
            <property>/foo</property>
            <property>/bar</property>
        </mybind>
        <mybind>
            <command>property-assign</command>
            <property>/foo</property>
            <property>/bar</property>
        </mybind>
        </PropertyList>
        )");

    auto bindings = readBindingList(config->getChildren("mybind"), globals->get_props());

    CPPUNIT_ASSERT_EQUAL(size_t{3}, bindings.size());

    fireBindingList(bindings);

    CPPUNIT_ASSERT_EQUAL(1, globals->get_props()->getIntValue("toggle-count"));
    CPPUNIT_ASSERT_EQUAL(2, globals->get_props()->getIntValue("assign-count"));

    fireBindingList(bindings);

    CPPUNIT_ASSERT_EQUAL(2, globals->get_props()->getIntValue("toggle-count"));
    CPPUNIT_ASSERT_EQUAL(4, globals->get_props()->getIntValue("assign-count"));
}

void SimgearBindingTests::testExpressionBinding()
{
    auto config = configFromString(R"(<?xml version="1.0" encoding="UTF-8"?>
        <PropertyList>
        <mybind>
            <command>expression</command>
            <property>/bar</property>
            <expression>
                <sum>
                    <binding-setting/>
                    <value>4</value>
                </sum>
            </expression>
        </mybind>
        </PropertyList>
        )");

    auto bindings = readBindingList(config->getChildren("mybind"), globals->get_props());

    SGPropertyNode_ptr args(new SGPropertyNode{});
    args->setDoubleValue("setting", 2.5);

    fireBindingList(bindings, args);

    CPPUNIT_ASSERT_DOUBLES_EQUAL(6.5, globals->get_props()->getDoubleValue("bar"), 1e-3);
}
