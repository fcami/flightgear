// SPDX-FileCopyrightText: 2019 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

#include <memory>

#include <simgear/props/props.hxx>

class SGGeod;
class GPS;

// The flight plan unit tests.
class RNAVProcedureTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(RNAVProcedureTests);

    CPPUNIT_TEST(testEGPH_TLA6C);
    CPPUNIT_TEST(testHeadingToAlt);
    CPPUNIT_TEST(testUglyHeadingToAlt);
    CPPUNIT_TEST(testLFKC_AJO1R);
    CPPUNIT_TEST(testTransitionsSID);
    CPPUNIT_TEST(testTransitionsSTAR);
    CPPUNIT_TEST(testLEBL_LARP2F);
	CPPUNIT_TEST(testIndexOf);

    CPPUNIT_TEST_SUITE_END();

    void setPositionAndStabilise(const SGGeod& g);

    GPS* setupStandardGPS(SGPropertyNode_ptr config = {},
        const std::string name = "gps", const int index = 0);
    void setupRouteManager();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    //void testBasic();

    void testEGPH_TLA6C();
    void testHeadingToAlt();
    void testUglyHeadingToAlt();
    void testLFKC_AJO1R();
    void testTransitionsSID();
    void testTransitionsSTAR();
    void testLEBL_LARP2F();
	void testIndexOf();
private:
    GPS* m_gps = nullptr;
    SGPropertyNode_ptr m_gpsNode;
};
