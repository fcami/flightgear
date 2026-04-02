// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Roman Ludwicki

#include "testVector.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/globals.hxx>

void VectorTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("vector");

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();

    FGTestApi::setUp::initStandardNasal(true);
    globals->get_subsystem_mgr()->postinit();
}

// Clean up after each test.
void VectorTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

void VectorTests::testRemoveString()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Remove one value
        var vector = ["test"];
        remove(vector, "test");

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Remove first value
        var vector = ["a", "b", "c"];
        remove(vector, "a");

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, ["b", "c"]);

        # Remove last value
        var vector = ["a", "b", "c"];
        remove(vector, "c");

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, ["a", "b"]);

        # Remove middle value
        var vector = ["a", "b", "c"];
        remove(vector, "b");

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, ["a", "c"]);

        # Nothing to remove
        var vector = ["a", "b", "c"];
        remove(vector, "d");

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, ["a", "b", "c"]);

        # Remove all the same value
        var vector = ["test", "test", "test"];
        remove(vector, "test");

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);
    )");
    CPPUNIT_ASSERT(ok);

    // Remove by nil value
    ok = FGTestApi::executeNasal(R"(
        var vector = ["a", "b", "c"];
        remove(vector, nil); # error, invalid argument
    )");
    CPPUNIT_ASSERT(!ok);
}

void VectorTests::testRemoveInt()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Remove one value
        var vector = [12];
        remove(vector, 12);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Remove first value
        var vector = [10, 11, 12];
        remove(vector, 10);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [11, 12]);

        # Remove last value
        var vector = [10, 11, 12];
        remove(vector, 12);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 11]);

        # Remove middle value
        var vector = [10, 11, 12];
        remove(vector, 11);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 12]);

        # Nothing to remove
        var vector = [10, 11, 12];
        remove(vector, 13);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        # Remove all the same value
        var vector = [10, 10, 10];
        remove(vector, 10);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);
    )");
    CPPUNIT_ASSERT(ok);
}

void VectorTests::testRemoveAt()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Remove one index
        var vector = [12];
        removeat(vector, 0);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Remove first index
        var vector = [10, 11, 12];
        removeat(vector, 0);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [11, 12]);

        # Remove last index
        var vector = [10, 11, 12];
        removeat(vector, 2);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 11]);

        # Remove middle index
        var vector = [10, 11, 12];
        removeat(vector, 1);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 12]);
    )");
    CPPUNIT_ASSERT(ok);

    //Remove out of range index
    ok = FGTestApi::executeNasal(R"(
        var vector = [10, 11, 12];
        removeat(vector, 3); # error, index out of range
    )");
    CPPUNIT_ASSERT(!ok);

    // Index as nil
    auto errs = FGTestApi::executeNasalExpectRuntimeErrors(R"(
        var vector = [10, 11, 12];
        removeat(vector, nil); # error, invalid argument
    )");
    CPPUNIT_ASSERT(errs.has_value());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), errs->size());
}

void VectorTests::testPop()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Pop from vector with zero items
        var vector = [];
        var result = pop(vector);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);
        unitTest.assert_equal(result, nil);

        # Pop from vector with one item
        var vector = [10];
        var result = pop(vector);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);
        unitTest.assert_equal(result, 10);

        # Pop from vector with two items
        var vector = [10, 11];
        var result = pop(vector);

        unitTest.assert_equal(size(vector), 1);
        unitTest.assert_equal(vector, [10]);
        unitTest.assert_equal(result, 11);

        # Pop from vector with many items
        var vector = [10, 11, 12, 13];
        var result = pop(vector);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);
        unitTest.assert_equal(result, 13);
    )");
    CPPUNIT_ASSERT(ok);
}

void VectorTests::testVecIndex()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Get index by value from empty vector
        var vector = [];
        var index = vecindex(vector, 10);

        unitTest.assert_equal(index, nil);

        # Get index by value from one item vector
        var vector = [10];
        var index = vecindex(vector, 10);

        unitTest.assert_equal(index, 0);

        # Get index for first value
        var vector = [10, 11, 12];
        var index = vecindex(vector, 10);

        unitTest.assert_equal(index, 0);

        # Get index for last value
        var vector = [10, 11, 12];
        var index = vecindex(vector, 12);

        unitTest.assert_equal(index, 2);

        # Get index for many same value
        var vector = [12, 12, 12];
        var index = vecindex(vector, 12);

        unitTest.assert_equal(index, 0);

        # Get index for mixed many same value
        var vector = [10, 12, 10, 12, 12];
        var index = vecindex(vector, 12);

        unitTest.assert_equal(index, 1);

        # Get index by value which not exist in vector
        var vector = [10, 11, 12];
        var index = vecindex(vector, 13);

        unitTest.assert_equal(index, nil);
    )");
    CPPUNIT_ASSERT(ok);
}

void VectorTests::testAppend()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Append one value to empty vector
        var vector = [];
        append(vector, 10);

        unitTest.assert_equal(size(vector), 1);
        unitTest.assert_equal(vector, [10]);

        # Append many values to empty vector
        var vector = [];
        append(vector, 10, 11, 12);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        # Append one value to not empty vector
        var vector = [10, 11, 12];
        append(vector, 13);

        unitTest.assert_equal(size(vector), 4);
        unitTest.assert_equal(vector, [10, 11, 12, 13]);

        # Append many values to not empty vector
        var vector = [10, 11, 12];
        append(vector, 10, 11, 12);

        unitTest.assert_equal(size(vector), 6);
        unitTest.assert_equal(vector, [10, 11, 12, 10, 11, 12]);

        # Append nil value to not empty vector
        var vector = [10, 11, 12];
        append(vector, nil);

        unitTest.assert_equal(size(vector), 4);
        unitTest.assert_equal(vector, [10, 11, 12, nil]);
    )");
    CPPUNIT_ASSERT(ok);
}

void VectorTests::testRange()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Range only with stop argument
        var vector = range(3);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [0, 1, 2]);

        # Range with start and stop arguments
        var vector = range(3, 6);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [3, 4, 5]);

        # Range with start, stop and step arguments - stops early producing 2 items
        var vector = range(3, 7, 2);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [3, 5]);

        # Range with start, stop and step arguments
        var vector = range(3, 8, 2);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [3, 5, 7]);

        # Range with start > stop
        var vector = range(10, 2);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Range empty vector (step 1 as default)
        var vector = range(0, 0);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Range 3 items vector with step rounded to 1.
        var vector = range(0, 3, 1.5); # step as 1.5 is rounded to 1

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [0, 1, 2]);

        # Range starts with negative number.
        var vector = range(-2, 3);

        unitTest.assert_equal(size(vector), 5);
        unitTest.assert_equal(vector, [-2, -1, 0, 1, 2]);
    )");
    CPPUNIT_ASSERT(ok);

    // Range empty vector with step 0
    ok = FGTestApi::executeNasal(R"(
        range(0, 0, 0); # error, invalid step argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Range 3 items vector with invalid step as -1.
    ok = FGTestApi::executeNasal(R"(
        range(0, 3, -1); # error, invalid step argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Range 3 items vector with invalid step as 0.
    ok = FGTestApi::executeNasal(R"(
        range(0, 3, 0); # error, invalid step argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Range 3 items vector with invalid step argument.
    ok = FGTestApi::executeNasal(R"(
        range(0, 3, 0.9); # error, invalid step argument (0.9 is rounded to 0)
    )");
    CPPUNIT_ASSERT(!ok);
}

void VectorTests::testSetSize()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Reduce vector size
        var vector = [10, 11, 12];
        setsize(vector, 2);

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 11]);

        # Reduce vector to zero
        var vector = [10, 11, 12];
        setsize(vector, 0);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Extend vector size
        var vector = [10, 11, 12];
        setsize(vector, 5);

        unitTest.assert_equal(size(vector), 5);
        unitTest.assert_equal(vector, [10, 11, 12, nil, nil]);

        # Extend empty vector size
        var vector = [];
        setsize(vector, 3);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [nil, nil, nil]);

        # Set zero size for empty vector
        var vector = [];
        setsize(vector, 0);

        unitTest.assert_equal(size(vector), 0);
        unitTest.assert_equal(vector, []);

        # Set the same size for vector
        var vector = [10, 11, 12];
        setsize(vector, 3);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        # Size as real number
        var vector = [10, 11, 12];
        setsize(vector, 2.99); # rounded to 2

        unitTest.assert_equal(size(vector), 2);
        unitTest.assert_equal(vector, [10, 11]);
    )");
    CPPUNIT_ASSERT(ok);

    // Reduce vector to negative value
    ok = FGTestApi::executeNasal(R"(
        var vector = [10, 11, 12];
        setsize(vector, -3); # error, size cannot be negative
    )");
    CPPUNIT_ASSERT(!ok);
}

void VectorTests::testSubVec()
{
    bool ok = FGTestApi::executeNasal(R"(
        # Get subvec from 0 to end
        var vector = [10, 11, 12];
        var sub = subvec(vector, 0);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        unitTest.assert_equal(size(sub), 3);
        unitTest.assert_equal(sub, [10, 11, 12]);

        # Get subvec from 1 to end
        var vector = [10, 11, 12];
        var sub = subvec(vector, 1);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        unitTest.assert_equal(size(sub), 2);
        unitTest.assert_equal(sub, [11, 12]);

        # Get subvec from 1 to 1
        var vector = [10, 11, 12];
        var sub = subvec(vector, 1, 1);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        unitTest.assert_equal(size(sub), 1);
        unitTest.assert_equal(sub, [11]);

        # Missing arguments, subvec return nil
        var vector = [10, 11, 12];
        var sub = subvec(vector);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        unitTest.assert_equal(sub, nil);

        # Length greater then vector size
        var vector = [10, 11, 12];
        var sub = subvec(vector, 0, 10);

        unitTest.assert_equal(size(vector), 3);
        unitTest.assert_equal(vector, [10, 11, 12]);

        unitTest.assert_equal(size(sub), 3);
        unitTest.assert_equal(sub, [10, 11, 12]);
    )");
    CPPUNIT_ASSERT(ok);

    // Get subvec from negative start
    ok = FGTestApi::executeNasal(R"(
        var vector = [10, 11, 12];
        subvec(vector, -1); # error, invalid argument
    )");
    CPPUNIT_ASSERT(!ok);

    // Get subvec with start > vector size
    ok = FGTestApi::executeNasal(R"(
        var vector = [10, 11, 12];
        subvec(vector, 4); # error, invalid argument
    )");
    CPPUNIT_ASSERT(!ok);
}
