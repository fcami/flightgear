// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

/**
 * Tests for PropertyChangeWebsocket — the websocket interface for watching
 * individual property nodes for value changes.
 *
 * The test pattern for websocket handlers:
 *   1. Create a MockWebsocketWriter to capture emitted JSON frames.
 *   2. Call ws.handleRequest() to deliver a JSON command (addListener / get / set).
 *   3. Drive the check/poll/uncheck cycle that the HTTP server normally runs.
 *   4. Assert the frames captured by the mock writer.
 *
 * Rate throttling is disabled by setting
 * /sim/http/property-websocket/update-interval-secs = 0 in setUp().
 */
class PropertyChangeWebsocketTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(PropertyChangeWebsocketTests);
    CPPUNIT_TEST(testAddListenerAndPoll);
    CPPUNIT_TEST(testGetCommand);
    CPPUNIT_TEST(testSetCommand);
    CPPUNIT_TEST(testNoChangePollEmitsNothing);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // addListener then poll → initial value frame emitted
    void testAddListenerAndPoll();
    // get command → value frame returned immediately
    void testGetCommand();
    // set command → property tree updated, no frame emitted
    void testSetCommand();
    // addListener, stabilise, no change → poll is silent
    void testNoChangePollEmitsNothing();
};
