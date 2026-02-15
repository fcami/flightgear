
// SPDX-FileCopyrightText: (C) 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "testNasalLib.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/globals.hxx>
#include <Main/util.hxx>
#include <Scripting/NasalSys.hxx>

#include <Main/FGInterpolator.hxx>

extern bool global_nasalMinimalInit;

// Set up function for each test.
void NasalLibTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("NasalGC");

    fgInitAllowedPaths();

    globals->get_subsystem_mgr()->add<FGInterpolator>();

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();

    global_nasalMinimalInit = true;
    globals->get_subsystem_mgr()->add<FGNasalSys>();

    globals->get_subsystem_mgr()->postinit();
}


// Clean up after each test.
void NasalLibTests::tearDown()
{
    global_nasalMinimalInit = false;
    FGTestApi::tearDown::shutdownTestGlobals();
}


// Test test
void NasalLibTests::testVector()
{
    bool ok = FGTestApi::executeNasal(R"(

        var v1 = ['apples', 'pears', 'lemons', 'strawberries'];
        append(v1, 'melons');
        unitTest.assert_equal(size(v1), 5);

        var item = removeat(v1, 2);
        unitTest.assert_equal(size(v1), 4);
        unitTest.assert_equal(item,'lemons');
        unitTest.assert_equal(v1[2], 'strawberries');

        remove(v1, 'carrots');
        unitTest.assert_equal(size(v1), 4); # nothing changes

        remove(v1, 'pears');
        unitTest.assert_equal(size(v1), 3);
        unitTest.assert_equal(v1[2], 'melons');

        unitTest.assert(contains(v1, 'pears') == 0);

    )");
    CPPUNIT_ASSERT(ok);
}

void NasalLibTests::testMember()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Hash with one parent
        var hash1 = { key1: 'value1' };
        var hash2 = { parents: [hash1], key2: 'value2' };

        unitTest.assert_equal(member(hash1, 'key1'), 'value1');
        unitTest.assert_equal(member(hash2, 'key1'), 'value1');
        unitTest.assert_equal(member(hash2, 'key2'), 'value2');
        unitTest.assert_equal(member(hash2, 'missing'), nil);


        # Hash with two parents
        hash1 = { key1: 'value1' };
        hash2 = { key2: 'value2' };
        var hash3 = { parents: [hash1, hash2], key3: 'value3' };

        unitTest.assert_equal(member(hash3, 'key1'), 'value1');
        unitTest.assert_equal(member(hash3, 'key2'), 'value2');
        unitTest.assert_equal(member(hash3, 'key3'), 'value3');
        unitTest.assert_equal(member(hash3, 'missing'), nil);


        # Hash with inheritance chain
        hash1 = { key1: 'value1' };
        hash2 = { parents: [hash1], key2: 'value2' };
        hash3 = { parents: [hash2], key3: 'value3' };

        unitTest.assert_equal(member(hash3, 'key1'), 'value1');
        unitTest.assert_equal(member(hash3, 'key2'), 'value2');
        unitTest.assert_equal(member(hash3, 'key3'), 'value3');
        unitTest.assert_equal(member(hash3, 'missing'), nil);


        # Tests with the same key name in multiple hashes
        hash1 = { key: 'hash1' };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2], key: 'hash3' };
        unitTest.assert_equal(member(hash3, 'key'), 'hash3');

        hash1 = { key: 'hash1' };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(member(hash3, 'key'), 'hash1');

        hash1 = { };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(member(hash3, 'key'), 'hash2');

        hash1 = { };
        hash2 = { };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(member(hash3, 'key'), nil);

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1], key: 'hash2' };
        hash3 = { parents: [hash2], key: 'hash3' };
        unitTest.assert_equal(member(hash3, 'key'), 'hash3');

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1], key: 'hash2' };
        hash3 = { parents: [hash2] };
        unitTest.assert_equal(member(hash3, 'key'), 'hash2');

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1] };
        hash3 = { parents: [hash2] };
        unitTest.assert_equal(member(hash3, 'key'), 'hash1');
    )");
    CPPUNIT_ASSERT(ok);

    // Invalid key argument
    ok = FGTestApi::executeNasal(R"(
        var hash = { 'key': 'value' };

        member(hash, nil); # error, invalid key argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Invalid hash argument
    ok = FGTestApi::executeNasal(R"(
        member(12, 'key'); # error, invalid hash argument
    )");
    CPPUNIT_ASSERT(!ok);
}

void NasalLibTests::testHasMember()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Hash with two parents
        var hash1 = { key1: 'value1' };
        var hash2 = { key2: 'value2' };
        var hash3 = { parents: [hash1, hash2], key3: 'value3' };

        unitTest.assert_equal(has_member(hash1, 'key1'), true);
        unitTest.assert_equal(has_member(hash3, 'key1'), true);
        unitTest.assert_equal(has_member(hash3, 'key2'), true);
        unitTest.assert_equal(has_member(hash3, 'key3'), true);
        unitTest.assert_equal(has_member(hash3, 'missing'), false);


        # Hash with inheritance chain
        hash1 = { key1: 'value1' };
        hash2 = { parents: [hash1], key2: 'value2' };
        hash3 = { parents: [hash2], key3: 'value3' };

        unitTest.assert_equal(has_member(hash3, 'key1'), true);
        unitTest.assert_equal(has_member(hash3, 'key2'), true);
        unitTest.assert_equal(has_member(hash3, 'key3'), true);
        unitTest.assert_equal(has_member(hash3, 'missing'), false);


        # Tests with the same key name in multiple hashes
        hash1 = { key: 'hash1' };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2], key: 'hash3' };
        unitTest.assert_equal(has_member(hash3, 'key'), true);

        hash1 = { key: 'hash1' };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(has_member(hash3, 'key'), true);

        hash1 = { };
        hash2 = { key: 'hash2' };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(has_member(hash3, 'key'), true);

        hash1 = { };
        hash2 = { };
        hash3 = { parents: [hash1, hash2] };
        unitTest.assert_equal(has_member(hash3, 'key'), false);

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1], key: 'hash2' };
        hash3 = { parents: [hash2], key: 'hash3' };
        unitTest.assert_equal(has_member(hash3, 'key'), true);

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1], key: 'hash2' };
        hash3 = { parents: [hash2] };
        unitTest.assert_equal(has_member(hash3, 'key'), true);

        hash1 = { key: 'hash1' };
        hash2 = { parents: [hash1] };
        hash3 = { parents: [hash2] };
        unitTest.assert_equal(has_member(hash3, 'key'), true);
    )");
    CPPUNIT_ASSERT(ok);

    // Invalid key argument
    ok = FGTestApi::executeNasal(R"(
        var hash = { 'key': 'value' };

        has_member(hash, nil); # error, invalid key argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Invalid hash argument
    ok = FGTestApi::executeNasal(R"(
        has_member(12, 'key'); # error, invalid hash argument
    )");
    CPPUNIT_ASSERT(!ok);
}
