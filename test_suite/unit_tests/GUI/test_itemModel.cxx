/*
 * SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "test_itemModel.hxx"
#include "config.h"


#include "cppunit/TestAssert.h"
#include "simgear/props/props.hxx"
#include "simgear/structure/exception.hxx"
#include "test_suite/FGTestApi/NavDataCache.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <initializer_list>
#include <simgear/canvas/Canvas.hxx>
#include <simgear/props/props_io.hxx>

#include <Main/fg_commands.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include <Canvas/canvas_mgr.hxx>
#include <Canvas/gui_mgr.hxx>

#include "test_suite/FGTestApi/DummyCanvasSystemAdapter.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

#include <GUI/AirportListModel.hxx>
#include <GUI/FGPUICompatDialog.hxx>
#include <GUI/ItemModel.hxx>
#include <GUI/NasalItemView.hxx>
#include <GUI/PUICompatObject.hxx>

#include <GUI/new_gui.hxx>

#include <Airports/airport.hxx>
#include <Scripting/NasalSys.hxx>

using namespace std::string_literals;
using namespace flightgear;

extern bool global_nasalMinimalInit;

class ModelObserver
{
public:
    struct Event {
        Event(ItemModel::Change c, int r, int count) : change(c),
                                                       row(r),
                                                       count(count)
        {
        }

        ItemModel::Change change;
        int row = -1, count = -1;
    };

    void f(ItemModel::Change t, size_t row, size_t count)
    {
        events.emplace_back(t, row, count);
    }

    std::vector<Event> events;
};

void ItemModelTests::setUp()
{
    global_nasalMinimalInit = false;

    FGTestApi::setUp::initTestGlobals("xmlui", "fr");
    FGTestApi::setUp::initNavDataCache(); // dialog loader uses the cache

    fgSetBool("/sim/menubar/enable", false);

    // Canvas needs loadxml command
    fgInitCommands();

    simgear::canvas::Canvas::setSystemAdapter(
        simgear::canvas::SystemAdapterPtr(new canvas::DummyCanvasSystemAdapter));

    auto sm = globals->get_subsystem_mgr();
    sm->add<CanvasMgr>();
    sm->add<NewGUI>();
    auto canvasGui = new GUIMgr;
    sm->add("CanvasGUI", canvasGui, SGSubsystemMgr::DISPLAY);


    sm->bind();
    sm->init();

    FGTestApi::setUp::initStandardNasal(true /* withCanvas */);

    sm->postinit();
}

void ItemModelTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

// trivial model for testing
class TrivialItemModel : public ItemModel
{
public:
    using Item = std::pair<std::string, std::string>;

    TrivialItemModel(const std::initializer_list<Item> items) : m_data(items)
    {
    }

    void insert(int index, const Item& item)
    {
        beginAddRows(index, 1);
        m_data.insert(m_data.begin() + index, item);
        endAddRows();
    }

    size_t count() const override
    {
        return m_data.size();
    }

    std::any dataAt(size_t index, const std::string& key) const override
    {
        if (index >= m_data.size()) {
            throw sg_exception("invalid index");
        }

        if (key == "label") {
            return m_data.at(index).first;
        }

        if (key == "value") {
            return m_data.at(index).second;
        }

        return {};
    }

private:
    std::vector<Item> m_data;
};

void ItemModelTests::testCallbacks()
{
    ItemModelRef m(new TrivialItemModel({{"Apples", "app"},
                                         {"Bananas", "ban"},
                                         {"Apples", "app2"},
                                         {"Carrots", "carrot"}}));

    CPPUNIT_ASSERT_EQUAL(std::any_cast<std::string>(m->dataAt(2, "label")), "Apples"s);

    // invalid indices
    CPPUNIT_ASSERT_THROW(m->dataAt(6, "value"), sg_exception);
    CPPUNIT_ASSERT_THROW(m->dataAt(99, "value"), sg_exception);

    // undefined keys
    CPPUNIT_ASSERT_EQUAL(m->dataAt(2, "velocity").has_value(), false);


    ModelObserver obs;
    m->addChangeCallback([&obs](ItemModel::Change t, size_t row, size_t count) {
        obs.f(t, row, count);
    });
}

void ItemModelTests::testPropertyModel()
{
    SGPropertyNode_ptr props(new SGPropertyNode());
    props->setStringValue("fruit[0]/legend", "Apple");
    props->setIntValue("fruit[0]/shelf", 41);
    props->setStringValue("fruit[2]/legend", "Lemon");
    props->setIntValue("fruit[2]/shelf", 42);
    props->setStringValue("fruit[3]/legend", "Lime");
    props->setIntValue("fruit[3]/shelf", 43);
    props->setStringValue("other/legend", "foofofof");

    SGSharedPtr<PropertyItemModel> m(new PropertyItemModel(props));
    m->setItemName("fruit");
    m->setLabelPath("legend");
    m->setValuePath("shelf");

    CPPUNIT_ASSERT_EQUAL(m->count(), size_t{3});
    CPPUNIT_ASSERT_EQUAL(std::any_cast<std::string>(m->dataAt(2, "label")), "Lime"s);
    CPPUNIT_ASSERT_EQUAL(std::any_cast<int>(m->dataAt(1, "value")), 42);

    CPPUNIT_ASSERT_THROW(m->dataAt(3, "value"), sg_exception);

    // insert updating
    ModelObserver obs;
    m->addChangeCallback([&obs](ItemModel::Change t, size_t row, size_t count) {
        obs.f(t, row, count);
    });

    props->setStringValue("fruit[1]/legend", "Grape");
    CPPUNIT_ASSERT_EQUAL(obs.events.size(), size_t{3});
    const auto ev0 = obs.events.at(0);
    CPPUNIT_ASSERT_EQUAL(ev0.change, ItemModel::Change::RowsWillBeAdded);
    CPPUNIT_ASSERT_EQUAL(ev0.row, 1);
    CPPUNIT_ASSERT_EQUAL(ev0.count, 1);

    const auto ev1 = obs.events.at(1);
    CPPUNIT_ASSERT_EQUAL(ev1.change, ItemModel::Change::RowsAdded);
    CPPUNIT_ASSERT_EQUAL(ev1.row, 1);
    CPPUNIT_ASSERT_EQUAL(ev1.count, 1);

    CPPUNIT_ASSERT_EQUAL(m->count(), size_t{4});
    obs.events.clear();

    props->setIntValue("fruit[1]/shelf", 99);
    const auto ev2 = obs.events.at(0);
    CPPUNIT_ASSERT_EQUAL(ev2.change, ItemModel::Change::Modified);
    CPPUNIT_ASSERT_EQUAL(ev2.row, 1);
    CPPUNIT_ASSERT_EQUAL(ev2.count, 1);

    CPPUNIT_ASSERT_EQUAL(std::any_cast<std::string>(m->dataAt(1, "label")), "Grape"s);

    // existing value updating
    obs.events.clear();
    props->setStringValue("fruit[3]/legend", "Pineapple");
    const auto ev3 = obs.events.at(0);
    CPPUNIT_ASSERT_EQUAL(ev3.change, ItemModel::Change::Modified);
    CPPUNIT_ASSERT_EQUAL(ev3.row, 3);
    CPPUNIT_ASSERT_EQUAL(ev3.count, 1);

    CPPUNIT_ASSERT_EQUAL(std::any_cast<std::string>(m->dataAt(3, "label")), "Pineapple"s);
}

void ItemModelTests::testNasalPropertyModel()
{
    fgSetString("/foo/vehicle[0]/name", "Ford");
    fgSetInt("/foo/vehicle[0]/id", 10);
    fgSetString("/foo/vehicle[1]/name", "Volkswagen");
    fgSetInt("/foo/vehicle[1]/id", 14);
    fgSetString("/foo/vehicle[4]/name", "Mercedes");
    fgSetInt("/foo/vehicle[4]/id", -100);

    bool ok = FGTestApi::executeNasal(R"(
        var r = props.globals.getNode("/foo");
        var m = gui.PropertyItemModel.new(r, "vehicle");
        m.labelPath = "name";
        m.valuePath = "id";

        unitTest.assert_equal(m.count, 3);
        unitTest.assert_equal(m.dataAt(2, "label"), "Mercedes");
        unitTest.assert_equal(m.dataAt(1, "value"), 14);

        r.getNode("vehicle[2]/name", 1).setValue("Audi");
        unitTest.assert_equal(m.count, 4);
        unitTest.assert_equal(m.dataAt(2, "label"), "Audi");
        unitTest.assert_equal(m.dataAt(3, "label"), "Mercedes");
    )");

    CPPUNIT_ASSERT(ok);
}

void ItemModelTests::testNasalModel()
{
    bool ok = FGTestApi::executeNasal(R"(
        var TestNasalModel = {
            _new: func()
            {
                var m = {
                    parents: [TestNasalModel],
                    labels: ['Apples', 'Bananas', 'Carrots'],
                    values: ['app1', 'bn', 'car']
                };
                return m;
            },

            count: func()
            {
                return size(me.labels);
            },

            modelData: func(row, key)
            {
                if (key == "label") return me.labels[row];
                if (key == "value") return me.values[row];
                return nil;
            }
        };

        var bm = TestNasalModel._new();
        var m = gui.NasalItemModel.new(bm);

        unitTest.assert_equal(m.count, 3);
        unitTest.assert_equal(m.dataAt(1, "label"), "Bananas");

    )");

    CPPUNIT_ASSERT(ok);
}

void ItemModelTests::testItemView()
{
    bool ok = FGTestApi::executeNasal(R"(
        var TestNasalModel = {
            _new: func()
            {
                var m = {
                    parents: [TestNasalModel],
                    labels: ['Apples', 'Bananas', 'Carrots', 'Donkeys', 'Elephants', 'Figs', 'Grapes', 'Horses', 'Ibis', 'Jackals', 'Kiwis', 'Llamas', 'Monkeys', 'Newts'],
                    values: ['app1', 'bn', 'car', 'don', 'ele', 'fig', 'gra', 'hor', 'ibi', 'jac', 'kiw', 'lla', 'mon', 'new'],
                    mass: [1.0, 1.1, 0.5, 200.0, 5000.0, 0.2, 0.3, 400.0, 2.0, 15.0, 0.4, 150.0, 30.0, 0.1]
                };
                return m;
            },

            count: func()
            {
                return size(me.labels);
            },

            modelData: func(row, key)
            {
                if (key == "label") return me.labels[row];
                if (key == "value") return me.values[row];
                if (key == "mass") return me.mass[row];
                return nil;
            },

            changeRow: func(row, label, value = nil, mass = nil)
            {
                me.labels[row] = label;
                if (value != nil) {
                    me.values[row] = value;
                }
                if (mass != nil) {
                    me.mass[row] = mass;
                }
                me.modelDataChanged(row, 1);
            }
        };

        var bm = TestNasalModel._new();
        var model = gui.NasalItemModel.new(bm);

        var TestView = {
            _new: func()
            {
                var m = {
                    parents: [TestView],
                    _delegateCount: 0,
                    _boundDelegateCount: 0
                };
                return m;
            },

            createDelegate: func()
            {
                var d = {
                    parents: [],
                    boundIndex: -1,
                    dataChangedCalled: false,
                    movedCalled: false,
                    unbindCalled: false,
                    text: "",

                    bind: func(index, modelData)
                    {
                        me.model = modelData;
                        me.view = modelData.view;
                        me.boundIndex = index;
                        modelData.view._boundDelegateCount += 1;
                        me.text = modelData.label;
                        logprint(LOG_INFO, "Delegate for index:" ~ index ~ " bound with label:" ~ me.text);
                    },

                    dataChanged: func()
                    {
                        logprint(LOG_INFO, "Delegate for index:" ~ me.boundIndex ~ " dataChanged called");
                        me.dataChangedCalled = true;
                        me.text = me.model.label;
                    },

                    moved: func()
                    {
                        me.movedCalled = true;
                    },

                    unbind: func()
                    {
                        logprint(LOG_INFO, "Delegate for index:" ~ me.boundIndex ~ " unbound");
                        me.unbindCalled = true;
                        me.model.view._boundDelegateCount -= 1;
                    }
                };

                me._delegateCount += 1;
                return d;
            },

            modelReset: func()
            {
            },

            visibleRowsChanged: func()
            {
            },

            scrollBarChanged: func()
            {
            },

            dataChanged: func(row)
            {
                logprint(LOG_INFO, "View dataChanged called for row:" ~ row);
            }
        };

        var v = gui.ItemView.new(TestView._new(), model);
        v.viewHeight = 320;
        v.delegateHeight = 60;
        v.cacheHeight = 0;

       # unitTest.assert_equal(6, v._delegateCount);
        var d2 = v.delegateForIndex(2);
        unitTest.assert_equal(d2.boundIndex, 2);
        unitTest.assert_equal(d2.model.index, 2);
        unitTest.assert_equal(d2.model.yPosition, 120);

        var d5 = v.delegateForIndex(5);
        unitTest.assert_equal(d5.boundIndex, 5);
        unitTest.assert_equal(d5.model.index, 5);
        unitTest.assert_equal(d5.model.yPosition, 300);

        unitTest.assert_equal(d5.text, "Figs");

        model.changeRow(5, "Ferrets", "frts", 3.0);
        unitTest.assert_equal(d5.text, "Ferrets");

        # scrolling
        v.viewOffset = 130;
        v.dumpDelegates();

        unitTest.assert_equal(6, v._boundDelegateCount);
        unitTest.assert_equal(nil, v.delegateForIndex(0));
        unitTest.assert_equal(nil, v.delegateForIndex(1));

        var d8 = v.delegateForIndex(8);
        unitTest.assert_equal(nil, d8);
        unitTest.assert_equal(nil, d8);

        unitTest.assert_equal(nil, v.delegateForIndex(9));

        unitTest.assert_equal(2, v.indexForViewPosition(5));
        unitTest.assert_equal(7, v.indexForViewPosition(310));

        v.viewOffset = 250;
        unitTest.assert_equal(nil, v.delegateForIndex(3));
        var d9 = v.delegateForIndex(9);
        unitTest.assert_equal(d9.boundIndex, 9);
        unitTest.assert_equal(d9.model.index, 9);
        unitTest.assert_equal(d9.model.yPosition, 540);
        unitTest.assert_equal(d9.text, "Jackals");
        unitTest.assert_equal(6, v._boundDelegateCount);
       # unitTest.assert_equal(6, v._delegateCount);

        v.dumpDelegates();

        # reset model to empty: all bound delegates must have unbind() called
        bm.labels = [];
        bm.values = [];
        bm.mass = [];
        model.reset();

        unitTest.assert_equal(0, v._boundDelegateCount);
        unitTest.assert_equal(nil, v.delegateForIndex(0));
    )");

    CPPUNIT_ASSERT(ok);
}

void ItemModelTests::testAirportListModelSearch()
{
    SGSharedPtr<AirportListModel> m(new AirportListModel);

    // A term shorter than 3 characters should leave the model empty without crashing.
    m->setSearchTerm("ED");
    CPPUNIT_ASSERT_EQUAL(size_t{0}, m->count());

    // Partial ICAO prefix "EDD" should match the family of German airports
    // (EDDF Frankfurt, EDDM Munich, EDDL Düsseldorf, …).
    m->setSearchTerm("EDD");
    CPPUNIT_ASSERT(m->count() > 0);

    // The label is "ICAO name" – both parts must be non-empty.
    for (size_t i = 0; i < m->count(); ++i) {
        auto label = std::any_cast<std::string>(m->dataAt(i, "label"));
        CPPUNIT_ASSERT(!label.empty());
    }

    // Well-known airports must be present.
    bool foundEDDF = false, foundEDDM = false;
    for (size_t i = 0; i < m->count(); ++i) {
        auto icao = std::any_cast<std::string>(m->dataAt(i, "icao"));
        if (icao == "EDDF") foundEDDF = true;
        if (icao == "EDDM") foundEDDM = true;
    }
    CPPUNIT_ASSERT_MESSAGE("EDDF (Frankfurt) must appear in EDD search", foundEDDF);
    CPPUNIT_ASSERT_MESSAGE("EDDM (Munich) must appear in EDD search", foundEDDM);

    // Changing to an unrelated term resets the results.
    m->setSearchTerm("ZZZZ_UNLIKELY");
    // May or may not be empty, but should not crash; count() must be defined.
    (void)m->count();
}

void ItemModelTests::testAirportListModelNameSearch()
{
    SGSharedPtr<AirportListModel> m(new AirportListModel);

    // "castle" should find airports whose name contains "castle"
    // (e.g. Newcastle Airport EGNT, Castle Donington EGNX, …).
    m->setSearchTerm("castle");
    CPPUNIT_ASSERT_MESSAGE("'castle' search should return at least one airport",
                           m->count() > 0);

    for (size_t i = 0; i < m->count(); ++i) {
        auto label = std::any_cast<std::string>(m->dataAt(i, "label"));
        std::string lower = label;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        CPPUNIT_ASSERT_MESSAGE("label should contain 'castle': " + label,
                               lower.find("castle") != std::string::npos);
    }

    // "new" should find airports such as Newcastle, New Orleans, Newark, etc.
    m->setSearchTerm("new");
    CPPUNIT_ASSERT_MESSAGE("'new' search should return at least one airport",
                           m->count() > 0);

    bool foundEGNT = false; // Newcastle Airport
    bool foundKEWR = false; // Newark Liberty International Airport
    bool foundKNEW = false; // New Orleans Lakefront Airport

    for (size_t i = 0; i < m->count(); ++i) {
        auto label = std::any_cast<std::string>(m->dataAt(i, "label"));
        std::string lower = label;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        CPPUNIT_ASSERT_MESSAGE("label should contain 'new': " + label,
                               lower.find("new") != std::string::npos);

        auto icao = std::any_cast<std::string>(m->dataAt(i, "icao"));
        if (icao == "EGNT") foundEGNT = true;
        if (icao == "KEWR") foundKEWR = true;
        if (icao == "KNEW") foundKNEW = true;
    }
    CPPUNIT_ASSERT_MESSAGE("EGNT (Newcastle) must appear in 'new' search", foundEGNT);
    CPPUNIT_ASSERT_MESSAGE("KEWR (Newark) must appear in 'new' search", foundKEWR);
    CPPUNIT_ASSERT_MESSAGE("KNEW (New Orleans) must appear in 'new' search", foundKNEW);
}

void ItemModelTests::testAirportListModelRecents()
{
    SGSharedPtr<AirportListModel> m(new AirportListModel);

    // No search term, no recents: model should be empty.
    CPPUNIT_ASSERT_EQUAL(size_t{0}, m->count());

    // Populate recents.
    FGAirportRef egnt = FGAirport::findByIdent("EGNT"); // Newcastle
    FGAirportRef eddf = FGAirport::findByIdent("EDDF"); // Frankfurt
    CPPUNIT_ASSERT_MESSAGE("EGNT must be in nav data", egnt != nullptr);
    CPPUNIT_ASSERT_MESSAGE("EDDF must be in nav data", eddf != nullptr);
    m->addRecentEntry(egnt);
    m->addRecentEntry(eddf);

    // Drive the model into a non-empty state first so that clearing the
    // search term is treated as a change and triggers useRecentAirports().
    m->setSearchTerm("EDD");
    CPPUNIT_ASSERT(m->count() > 0);

    // Clearing the search term must show the recent airports.
    ModelObserver obs;
    m->addChangeCallback([&obs](ItemModel::Change t, size_t row, size_t count) {
        obs.f(t, row, count);
    });

    m->setSearchTerm("");
    CPPUNIT_ASSERT_EQUAL(size_t{2}, m->count());

    // The recents callback must have fired a Reset.
    CPPUNIT_ASSERT(!obs.events.empty());
    CPPUNIT_ASSERT_EQUAL(ItemModel::Change::Reset, obs.events.front().change);

    // Entries must appear in the order they were added.
    CPPUNIT_ASSERT_EQUAL("EGNT"s, std::any_cast<std::string>(m->dataAt(0, "icao")));
    CPPUNIT_ASSERT_EQUAL("EDDF"s, std::any_cast<std::string>(m->dataAt(1, "icao")));

    // Labels must be non-empty and contain the ICAO code.
    auto label0 = std::any_cast<std::string>(m->dataAt(0, "label"));
    CPPUNIT_ASSERT(label0.find("EGNT") != std::string::npos);
    auto label1 = std::any_cast<std::string>(m->dataAt(1, "label"));
    CPPUNIT_ASSERT(label1.find("EDDF") != std::string::npos);
}
