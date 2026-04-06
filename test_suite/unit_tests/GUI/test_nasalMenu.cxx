/*
 * SPDX-FileCopyrightText: 2026 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_nasalMenu.hxx"
#include "config.h"

#include "cppunit/TestAssert.h"
#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <simgear/canvas/Canvas.hxx>
#include <simgear/props/props.hxx>

#include <Main/fg_commands.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include <Canvas/canvas_mgr.hxx>
#include <Canvas/gui_mgr.hxx>

#include "test_suite/FGTestApi/DummyCanvasSystemAdapter.hxx"

#include <GUI/new_gui.hxx>

using namespace std::string_literals;

extern bool global_nasalMinimalInit;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Populate /sim/menubar/default with a minimal two-menu structure:
 *
 *   menu[0]  name="test-file-menu"  label="Test File"
 *     item[0]  name="test-reset"  label="Test Reset"   enabled=true
 *     item[1]  name="test-exit"   label="Test Exit"    enabled=false
 *
 *   menu[1]  name="test-help-menu"  label="Test Help"
 *     item[0]  name="test-about"  label="Test About"   enabled=true
 */
static void populateTestMenubar()
{
    auto menubarNode = fgGetNode("/sim/menubar/default", true);

    auto fileMenu = menubarNode->addChild("menu");
    fileMenu->setStringValue("name", "test-file-menu");
    fileMenu->setStringValue("label", "Test File");

    auto item0 = fileMenu->addChild("item");
    item0->setStringValue("name", "test-reset");
    item0->setStringValue("label", "Test Reset");

    auto item1 = fileMenu->addChild("item");
    item1->setStringValue("name", "test-exit");
    item1->setStringValue("label", "Test Exit");
    item1->setBoolValue("enabled", false);

    auto helpMenu = menubarNode->addChild("menu");
    helpMenu->setStringValue("name", "test-help-menu");
    helpMenu->setStringValue("label", "Test Help");

    auto helpItem0 = helpMenu->addChild("item");
    helpItem0->setStringValue("name", "test-about");
    helpItem0->setStringValue("label", "Test About");
}

// ---------------------------------------------------------------------------

void NasalMenuTests::setUp()
{
    global_nasalMinimalInit = false;

    FGTestApi::setUp::initTestGlobals("nasal-menu");
    FGTestApi::setUp::initNavDataCache(); // dialog loader uses the cache

    populateTestMenubar();

    // Force the NasalMenuBar implementation (not the native OS bars).
    fgSetBool("/sim/menubar/native", false);

    // Canvas needs loadxml command.
    fgInitCommands();

    simgear::canvas::Canvas::setSystemAdapter(
        simgear::canvas::SystemAdapterPtr(new canvas::DummyCanvasSystemAdapter));

    auto sm = globals->get_subsystem_mgr();
    sm->add<CanvasMgr>();
    sm->add<NewGUI>();
    sm->add("CanvasGUI", new GUIMgr, SGSubsystemMgr::DISPLAY);

    sm->bind();
    sm->init();

    FGTestApi::setUp::initStandardNasal(true /* withCanvas */);

    // Override gui._createMenuBar before postinit so we can capture the
    // menubar ghost in a global variable without needing a live Canvas window.
    bool ok = FGTESTAPI_EXECUTE_NASAL(R"(
        globals._testMenubarGhost = nil;
        gui._createMenuBar = func(ghost) {
            globals._testMenubarGhost = ghost;
        };
    )");
    ;
    CPPUNIT_ASSERT(ok);

    sm->postinit();

    // Sanity check – the hook should have fired.
    ok = FGTESTAPI_EXECUTE_NASAL(R"(
        unitTest.assert(_testMenubarGhost != nil, "menubar ghost was not captured");
    )");
    ;
    CPPUNIT_ASSERT(ok);
}

void NasalMenuTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// ---------------------------------------------------------------------------
// Test 1 – basic structure and fields exposed to Nasal
// ---------------------------------------------------------------------------

void NasalMenuTests::testBasicStructure()
{
    bool ok = FGTESTAPI_EXECUTE_NASAL(R"(
        var bar = _testMenubarGhost;

        # Two top-level menus.
        var menus = bar.menus;
        unitTest.assert_equal(size(menus), 2, "menu count");

        # First menu identity.
        var fileMenu = menus[0];
        unitTest.assert_equal(fileMenu.name,    "test-file-menu", "menu[0].name");
        unitTest.assert_equal(fileMenu.label,   "Test File",      "menu[0].label");
        unitTest.assert_equal(fileMenu.enabled, 1,                "menu[0].enabled");

        # Second menu.
        var helpMenu = menus[1];
        unitTest.assert_equal(helpMenu.name,  "test-help-menu", "menu[1].name");
        unitTest.assert_equal(helpMenu.label, "Test Help",       "menu[1].label");

        # Items in the first menu.
        var items = fileMenu.items;
        unitTest.assert_equal(size(items), 2, "file menu item count");

        var resetItem = items[0];
        unitTest.assert_equal(resetItem.name,      "test-reset", "item[0].name");
        unitTest.assert_equal(resetItem.label,     "Test Reset", "item[0].label");
        unitTest.assert_equal(resetItem.enabled,   1,            "item[0].enabled");
        unitTest.assert_equal(resetItem.separator, 0,            "item[0].separator");
        unitTest.assert_equal(resetItem.checkable, 0,            "item[0].checkable");

        var exitItem = items[1];
        unitTest.assert_equal(exitItem.name,    "test-exit", "item[1].name");
        unitTest.assert_equal(exitItem.label,   "Test Exit", "item[1].label");
        unitTest.assert_equal(exitItem.enabled, 0,           "item[1].enabled");

        # Items in the second menu.
        var helpItems = helpMenu.items;
        unitTest.assert_equal(size(helpItems),     1,            "help menu item count");
        unitTest.assert_equal(helpItems[0].name,   "test-about", "help item[0].name");

        # MenuChangeKind constants must be exposed on gui.Menu.
        unitTest.assert_equal(gui.Menu.ChildAdded,   0, "gui.Menu.ChildAdded");
        unitTest.assert_equal(gui.Menu.ChildRemoved, 1, "gui.Menu.ChildRemoved");
        unitTest.assert_equal(gui.Menu.Updated,      2, "gui.Menu.Updated");
    )");
    CPPUNIT_ASSERT(ok);
}

// ---------------------------------------------------------------------------
// Test 2 – changes to config properties trigger the Updated callback
// ---------------------------------------------------------------------------

void NasalMenuTests::testUpdateCallback()
{
    // Register callbacks on the first menu and on its first item, then
    // trigger changes via property-tree writes and verify the callbacks fire.
    bool ok = FGTESTAPI_EXECUTE_NASAL(R"(
        var fileMenu  = _testMenubarGhost.menus[0];
        var resetItem = fileMenu.items[0];

        # --- menu-level callback ---
        var menuCallCount = 0;
        var menuLastKind  = -1;
        var menuLastIndex = -1;

        fileMenu.addChangedCallback(func(kind, index) {
            menuCallCount += 1;
            menuLastKind   = kind;
            menuLastIndex  = index;
        });

        # Disable the menu via the property tree.
        var menuNode = props.globals.getNode("/sim/menubar/default/menu[0]");
        menuNode.getNode("enabled").setBoolValue(0);

        unitTest.assert_equal(menuCallCount, 1,                "menu callback fire count");
        unitTest.assert_equal(menuLastKind,  gui.xml.Menu.Updated, "menu callback kind");
        unitTest.assert_equal(menuLastIndex, -1,               "menu callback index");

        # --- item-level callback ---
        var itemCallCount = 0;
        var itemLastKind  = -1;

        resetItem.addChangedCallback(func(kind, index) {
            itemCallCount += 1;
            itemLastKind   = kind;
        });

        # Change the enabled state of the first item.
        var itemNode = menuNode.getNode("item[0]");
        itemNode.getNode("enabled").setBoolValue(0);

        unitTest.assert_equal(itemCallCount, 1,                "item callback fire count");
        unitTest.assert_equal(itemLastKind,  gui.Menu.Updated, "item callback kind");

        # The ghost accessor must reflect the new state.
        unitTest.assert_equal(resetItem.enabled, 0, "item.enabled after disable");
    )");
    CPPUNIT_ASSERT(ok);
}

// ---------------------------------------------------------------------------
// Test 3 – dynamic item insertion and removal trigger ChildAdded / ChildRemoved
// ---------------------------------------------------------------------------

void NasalMenuTests::testDynamicItemAddRemove()
{
    bool ok = FGTESTAPI_EXECUTE_NASAL(R"(
        var fileMenu = _testMenubarGhost.menus[0];

        var events = [];   # collect (kind, index) pairs in order

        fileMenu.addChangedCallback(func(kind, index) {
            append(events, {kind: kind, index: index});
        });

        var menuNode = props.globals.getNode("/sim/menubar/default/menu[0]");

        # --- add a new item ---
        # addChild("item") appends item[2] (indices 0 and 1 already exist).
        var newItemNode = menuNode.addChild("item");
        newItemNode.getNode("name",  1).setValue("test-new");
        newItemNode.getNode("label", 1).setValue("Test New");

        unitTest.assert_equal(size(events),     1,                   "event count after add");
        unitTest.assert_equal(events[0].kind,    gui.xml.Menu.ChildAdded, "add event kind");
        unitTest.assert_equal(events[0].index,   newItemNode.getIndex(), "add event index");

        # The item should now appear in the items vector.
        unitTest.assert_equal(size(fileMenu.items), 3, "item count after add");

        # --- remove the newly added item ---
        menuNode.removeChild("item", newItemNode.getIndex());

        unitTest.assert_equal(size(events),   2,                      "event count after remove");
        unitTest.assert_equal(events[1].kind, gui.xml.Menu.ChildRemoved,  "remove event kind");

        # ChildRemoved fires BEFORE the internal vector is updated, so the
        # callback should have been invoked while the item was still present.
        # After the remove returns, the vector must be back to 2.
        unitTest.assert_equal(size(fileMenu.items), 2, "item count after remove");

        # --- verify ordering: insert at an explicit index between item[0] and item[1] ---
        # Create item[1] with an index between the two existing items by first
        # removing item[1], giving item[0] and then re-adding two items.
        # Simpler: create a fresh menu and verify insertions are ordered.
        var barNode = props.globals.getNode("/sim/menubar/default");
        var testMenu2 = barNode.addChild("menu");
        testMenu2.getNode("name", 1).setValue("test-order-menu");

        var m2ghost = nil;
        foreach (var m; _testMenubarGhost.menus) {
            if (m.name == "test-order-menu") { m2ghost = m; break; }
        }
        unitTest.assert(m2ghost != nil, "Could not find test-order-menu ghost");

        # Insert item[5] first, then item[2].  The items vector should be
        # ordered item[2] before item[5].
        var hi = testMenu2.getChild("item", 5, 1);
        hi.getNode("name", 1).setValue("test-hi");
        var lo = testMenu2.getChild("item", 2, 1);
        lo.getNode("name", 1).setValue("test-lo");

        var orderedItems = m2ghost.items;
        unitTest.assert_equal(size(orderedItems),    2,          "order-test menu item count");
        unitTest.assert_equal(orderedItems[0].name,  "test-lo",  "first ordered item should have index 2");
        unitTest.assert_equal(orderedItems[1].name,  "test-hi",  "second ordered item should have index 5");
    )");
    CPPUNIT_ASSERT(ok);
}
