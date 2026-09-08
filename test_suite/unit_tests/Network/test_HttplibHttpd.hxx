// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <Network/http/HttplibHttpd.hxx>

#include <simgear/misc/sg_path.hxx>

#include <atomic>
#include <memory>
#include <thread>

/**
 * Integration tests for HttplibHttpd: unlike the other tests in this
 * directory, these spin up a real HttplibHttpd (bound to a real socket)
 * and hit it with a real HTTP/WebSocket client.
 */
class HttplibHttpdTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(HttplibHttpdTests);
    CPPUNIT_TEST(testStaticFile);
    CPPUNIT_TEST(testUnknownRouteFallsThroughToStaticFiles);
    CPPUNIT_TEST(testJsonHandler);
    CPPUNIT_TEST(testPropertyHandler);
    CPPUNIT_TEST(testRunHandler);
    CPPUNIT_TEST(testPkgHandlerWithoutPackageRoot);
    CPPUNIT_TEST(testPkgHandlerWithPackageRoot);
    CPPUNIT_TEST(testNavdbHandler);
    CPPUNIT_TEST(testFlightHistoryHandler);
    CPPUNIT_TEST(testPropertyListenerWebsocket);
    CPPUNIT_TEST(testPropertyTreeMirrorWebsocket);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // GET a plain file out of the configured document root.
    void testStaticFile();
    // A URI matching no handler prefix falls through to static files
    // (mirrors MongooseHttpd's MG_FALSE fallthrough).
    void testUnknownRouteFallsThroughToStaticFiles();
    // GET an existing property via /json/.
    void testJsonHandler();
    // GET the property tree root via /props/.
    void testPropertyHandler();
    // GET /run?value=<command> for a command registered in the test.
    void testRunHandler();
    // GET /pkg/path with no package root configured -> graceful 500.
    void testPkgHandlerWithoutPackageRoot();
    // GET /pkg/path with a package root configured -> 200.
    void testPkgHandlerWithPackageRoot();
    // GET /navdb/?q=airport&id=<unknown> -> empty but valid GeoJSON.
    void testNavdbHandler();
    // GET /flighthistory/track.json with the FGFlightHistory subsystem
    void testFlightHistoryHandler();
    // Connect to /PropertyListener, addListener, change the property,
    // and receive the pushed notification.
    void testPropertyListenerWebsocket();
    // Connect to /PropertyTreeMirror/<path> and receive the initial
    // subtree snapshot, then a change notification.
    void testPropertyTreeMirrorWebsocket();

private:
    std::unique_ptr<flightgear::HttplibHttpd> _httpd;
    SGPath _docRoot;
    int _port = 0;

    std::thread _updatePumpThread;
    std::atomic<bool> _stopUpdatePump{false};

    // Blocks (with a generous real-time budget) until the server is
    // actually accepting connections, using a real client rather than
    // any internal server API.
    void waitUntilServerIsUp();
};
