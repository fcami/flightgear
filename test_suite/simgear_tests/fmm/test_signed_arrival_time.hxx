// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Stuart Buchanan
//
// CppUnit port of a subset of the upstream thinks::fast_marching_method
// test suite (https://github.com/thinks/fast-marching-method,
// tests/signed_arrival_time_test.cpp plus one test from
// tests/eikonal_solvers_test.cpp's DistanceSolverTest fixture), covering
// the fmm::SignedArrivalTime<float, 3> + fmm::DistanceSolver instantiation
// actually used by FGClouds::generateSDF() (Environment/fgclouds.cxx) and
// SGVoxelTextureCloud::generateCloud() (simgear/scene/sky/newcloud.cxx).
// Not a full port: the upstream suite is typed/templated across 6
// scalar-type/dimension combinations and also covers several Eikonal
// solver classes (UniformSpeedEikonalSolver, VaryingSpeedEikonalSolver,
// their HighAccuracy variants) that nothing in this codebase instantiates -
// this only covers the <float, 3> + DistanceSolver combination actually
// used, plus DistanceSolver's own dedicated test. ContainedComponentThrows
// is intentionally not ported: it aborts against this fork specifically
// because the fork disabled the exception it expects (see the "contained
// component" comment at the relevant check in fast_marching_method.hpp).

#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class SignedArrivalTimeTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(SignedArrivalTimeTests);
    CPPUNIT_TEST(testZeroElementInGridSizeThrows);
    CPPUNIT_TEST(testEmptyBoundaryThrows);
    CPPUNIT_TEST(testFullGridBoundaryIndicesThrows);
    CPPUNIT_TEST(testDuplicateBoundaryIndicesThrows);
    CPPUNIT_TEST(testBoundaryIndexOutsideGridThrows);
    CPPUNIT_TEST(testBoundaryIndicesAndTimesSizeMismatchThrows);
    CPPUNIT_TEST(testInvalidBoundaryTimeThrows);
    CPPUNIT_TEST(testEikonalSolverFailThrows);
    CPPUNIT_TEST(testDistanceSolverInvalidGridSpacingThrows);
    CPPUNIT_TEST(testDifferentUniformSpeed);
    CPPUNIT_TEST(testNonUniformGridSpacing);
    CPPUNIT_TEST(testBoxBoundary);
    CPPUNIT_TEST(testCheckerboard);
    CPPUNIT_TEST(testOverlappingBoxes);
    CPPUNIT_TEST(testPointSourceAccuracy);
    CPPUNIT_TEST(testMaxDistanceCap);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    // Ported from upstream - input validation. Confirms the FlightGear fork
    // hasn't drifted from the exact exception messages these rely on.
    void testZeroElementInGridSizeThrows();
    void testEmptyBoundaryThrows();
    void testFullGridBoundaryIndicesThrows();
    void testDuplicateBoundaryIndicesThrows();
    void testBoundaryIndexOutsideGridThrows();
    void testBoundaryIndicesAndTimesSizeMismatchThrows();
    void testInvalidBoundaryTimeThrows();
    void testEikonalSolverFailThrows();

    // Ported from upstream eikonal_solvers_test.cpp's DistanceSolverTest
    // fixture (not signed_arrival_time_test.cpp like the rest of this file)
    // - fmm::DistanceSolver is the actual solver type FGClouds::
    // generateSDF()/SGVoxelTextureCloud::generateCloud() instantiate, unlike
    // every other class in that file (UniformSpeedEikonalSolver,
    // VaryingSpeedEikonalSolver, and their HighAccuracy variants), which
    // aren't ported since nothing in this codebase uses them.
    void testDistanceSolverInvalidGridSpacingThrows();

    // Ported from upstream - correctness of the solve itself.
    void testDifferentUniformSpeed();
    void testNonUniformGridSpacing();
    void testBoxBoundary();
    void testCheckerboard();
    void testOverlappingBoxes();
    void testPointSourceAccuracy();

    // New - not present upstream. Covers the max_distance early-exit cap
    // added to MarchNarrowBand/ArrivalTime/SignedArrivalTime for the
    // non-repeating cloud field performance work (see
    // /sim/rendering/hdr/clouds/sdf-max-distance-voxels in fgclouds.cxx).
    void testMaxDistanceCap();
};
