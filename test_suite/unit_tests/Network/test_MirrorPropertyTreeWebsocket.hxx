// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

/**
 * Tests for MirrorPropertyTreeWebsocket — the websocket that mirrors an
 * entire property sub-tree to a connected client.
 *
 * The test pattern mirrors the server's update loop:
 *   1. Build a property sub-tree.
 *   2. Construct MirrorPropertyTreeWebsocket (registers a recursive listener).
 *   3. Call ws.poll() — the listener accumulates changes and the websocket
 *      serialises them as a JSON frame via MockWebsocketWriter.
 *   4. Mutate the sub-tree and call ws.poll() again to check delta frames.
 *
 * The send-rate throttle is disabled by setting
 * /sim/http/mirror-websocket/update-interval-ms = 0 before constructing.
 */
class MirrorPropertyTreeWebsocketTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(MirrorPropertyTreeWebsocketTests);
    CPPUNIT_TEST(testInitialSubtreeSent);
    CPPUNIT_TEST(testNewChildSent);
    CPPUNIT_TEST(testValueChangeSent);
    CPPUNIT_TEST(testRemovedChildSent);
    CPPUNIT_TEST(testDeleteAndReAddAppearsAsChange);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // On first poll after construction the whole existing sub-tree is sent
    void testInitialSubtreeSent();
    // A child added after construction appears in the next poll frame
    void testNewChildSent();
    // A value change in the sub-tree appears in the next poll frame
    void testValueChangeSent();
    // A child removed from the sub-tree appears in the next poll frame
    void testRemovedChildSent();
    // A child removed and immediately re-added appears as a value change,
    // not as a separate remove + create pair
    void testDeleteAndReAddAppearsAsChange();
};
