/*
 * SPDX-FileCopyrightText: (C) 2022 Lars Toenning <dev@ltoenning.de>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "test_HttplibHttpd.hxx"
#include "test_JsonUriHandler.hxx"
#include "test_MirrorPropertyTreeWebsocket.hxx"
#include "test_PropertyChangeWebsocket.hxx"

// HTTP handler and websocket unit tests
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(JsonUriHandlerTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PropertyChangeWebsocketTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MirrorPropertyTreeWebsocketTests, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(HttplibHttpdTests, "Unit tests");
#if defined(ENABLE_SWIFT)

#include "test_swiftAircraftManager.hxx"
#include "test_swiftService.hxx"

// Set up the unit tests.
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SwiftAircraftManagerTest, "Unit tests");
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SwiftServiceTest, "Unit tests");

#endif
