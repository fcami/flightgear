// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Stuart Buchanan

#include "test_signed_arrival_time.hxx"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fast_marching_method.hpp>

#include "fmm_test_util.hxx"

namespace {
namespace fmm = thinks::fast_marching_method;

// FlightGear only ever instantiates fmm::SignedArrivalTime with these
// parameters (see FGClouds::generateSDF() and
// SGVoxelTextureCloud::generateCloud()) - fixed rather than typed/templated
// across scalar type and dimension the way the upstream gtest suite is,
// since CppUnit has no typed-test facility and the extra generality isn't
// exercised by any real caller in this codebase.
typedef float ScalarType;
constexpr std::size_t kDimension = 3;
typedef fmm::UniformSpeedEikonalSolver<ScalarType, kDimension> EikonalSolverType;
typedef fmm::DistanceSolver<ScalarType, kDimension> DistanceSolverType;

// Runs @a func and asserts it throws ExceptionT with exactly @a expected_what
// - CPPUNIT_ASSERT_THROW alone can't check the message, and the exact
// wording is what these tests are actually pinning (a silent wording drift
// in the vendored header would otherwise go unnoticed).
template <typename ExceptionT, typename F>
void AssertThrowsWithMessage(F const func, std::string const& expected_what)
{
    try {
        func();
    } catch (ExceptionT const& e) {
        CPPUNIT_ASSERT_EQUAL(expected_what, std::string(e.what()));
        return;
    }
    CPPUNIT_FAIL("expected " + expected_what + " to be thrown, but nothing was");
}
} // namespace

void SignedArrivalTimeTests::setUp() {}
void SignedArrivalTimeTests::tearDown() {}

void SignedArrivalTimeTests::testZeroElementInGridSizeThrows()
{
    for (auto i = std::size_t{0}; i < kDimension; ++i) {
        auto grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
        grid_size[i] = 0;
        auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

        auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
        boundary_indices.push_back(fmm_test::FilledArray<kDimension>(std::int32_t{0}));
        auto const boundary_distances = std::vector<ScalarType>(1, ScalarType{1});

        auto expected = std::stringstream();
        expected << "invalid size: " << fmm_test::ToString(grid_size);

        AssertThrowsWithMessage<std::invalid_argument>(
            [&]() {
                fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_distances,
                                       EikonalSolverType(grid_spacing, ScalarType{1}));
            },
            expected.str());
    }
}

void SignedArrivalTimeTests::testEmptyBoundaryThrows()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});
    auto const boundary_indices = std::vector<std::array<std::int32_t, kDimension>>{};
    auto const boundary_distances = std::vector<ScalarType>{};

    AssertThrowsWithMessage<std::invalid_argument>(
        [&]() {
            fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_distances,
                                   EikonalSolverType(grid_spacing, ScalarType{1}));
        },
        "empty boundary condition");
}

void SignedArrivalTimeTests::testFullGridBoundaryIndicesThrows()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    bool has_next = true;
    while (has_next) {
        boundary_indices.push_back(index_iter.index());
        has_next = index_iter.Next();
    }
    auto const boundary_distances = std::vector<ScalarType>(fmm_test::LinearSize(grid_size), ScalarType{1});

    AssertThrowsWithMessage<std::invalid_argument>(
        [&]() {
            fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_distances,
                                   EikonalSolverType(grid_spacing, ScalarType{1}));
        },
        "full grid boundary");
}

void SignedArrivalTimeTests::testDuplicateBoundaryIndicesThrows()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    boundary_indices.push_back(index_iter.index());
    boundary_indices.push_back(index_iter.index()); // Same index.
    auto const boundary_distances = std::vector<ScalarType>(2, ScalarType{1});

    auto expected = std::stringstream();
    expected << "duplicate boundary index: " << fmm_test::ToString(index_iter.index());

    AssertThrowsWithMessage<std::invalid_argument>(
        [&]() {
            fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_distances,
                                   EikonalSolverType(grid_spacing, ScalarType{1}));
        },
        expected.str());
}

void SignedArrivalTimeTests::testBoundaryIndexOutsideGridThrows()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    boundary_indices.push_back(index_iter.index());
    boundary_indices.push_back(fmm_test::FilledArray<kDimension>(std::int32_t{-1})); // Outside.
    auto const boundary_times = std::vector<ScalarType>(2, ScalarType{1});

    auto expected = std::stringstream();
    expected << "boundary index outside grid - "
             << "index: " << fmm_test::ToString(boundary_indices.back()) << ", "
             << "grid size: " << fmm_test::ToString(grid_size);

    AssertThrowsWithMessage<std::invalid_argument>(
        [&]() {
            fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                   EikonalSolverType(grid_spacing, ScalarType{1}));
        },
        expected.str());
}

void SignedArrivalTimeTests::testBoundaryIndicesAndTimesSizeMismatchThrows()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    boundary_indices.push_back(index_iter.index());
    index_iter.Next();
    boundary_indices.push_back(index_iter.index());
    auto const boundary_times = std::vector<ScalarType>(3, ScalarType{1}); // Two indices, three times.

    AssertThrowsWithMessage<std::invalid_argument>(
        [&]() {
            fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                   EikonalSolverType(grid_spacing, ScalarType{1}));
        },
        "boundary indices[2] / boundary times[3] size mismatch");
}

void SignedArrivalTimeTests::testInvalidBoundaryTimeThrows()
{
    auto const invalid_times = std::array<ScalarType, 3>{
        {std::numeric_limits<ScalarType>::max(), -std::numeric_limits<ScalarType>::max(),
         std::numeric_limits<ScalarType>::quiet_NaN()}};

    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    for (auto const invalid_time : invalid_times) {
        auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
        auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
        boundary_indices.push_back(index_iter.index());
        auto const boundary_times = std::vector<ScalarType>(1, invalid_time);

        auto expected = std::stringstream();
        expected << "invalid boundary time: " << invalid_time;

        AssertThrowsWithMessage<std::invalid_argument>(
            [&]() {
                fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                       EikonalSolverType(grid_spacing, ScalarType{1}));
            },
            expected.str());
    }
}

void SignedArrivalTimeTests::testEikonalSolverFailThrows()
{
    // No-fallback solver, deliberately fed a scenario with a tiny value in
    // one direction and a huge value in another - unresolvable gradient.
    typedef fmm::UniformSpeedEikonalSolver<ScalarType, kDimension, false> NoFallbackSolverType;

    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    boundary_indices.push_back(fmm_test::FilledArray<kDimension>(std::int32_t{0}));
    boundary_indices.push_back(fmm_test::FilledArray<kDimension>(std::int32_t{1}));
    auto boundary_times = std::vector<ScalarType>();
    boundary_times.push_back(ScalarType{1000});
    boundary_times.push_back(ScalarType{1});

    // "invalid arrival time (distance)" is exactly 31 characters - the
    // message continues with per-neighbor diagnostic details that aren't
    // worth pinning here, so only the fixed prefix is checked (matches the
    // upstream test's own substr(0, 31)).
    std::string const kExpectedPrefix = "invalid arrival time (distance)";
    try {
        fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                               NoFallbackSolverType(grid_spacing, ScalarType{1}));
    } catch (std::runtime_error const& e) {
        std::string const what(e.what());
        CPPUNIT_ASSERT_EQUAL(kExpectedPrefix, what.substr(0, kExpectedPrefix.size()));
        return;
    }
    CPPUNIT_FAIL("expected std::runtime_error to be thrown, but nothing was");
}

void SignedArrivalTimeTests::testDistanceSolverInvalidGridSpacingThrows()
{
    // DistanceSolver's constructor takes one scalar dx (broadcast to every
    // dimension internally - see fast_marching_method.hpp's DistanceSolver),
    // unlike the other solver classes' per-dimension grid_spacing array, so
    // there's no per-dimension variation to loop over here the way the
    // upstream UniformSpeedEikonalSolverTest does.
    auto const invalid_dx_values = std::array<ScalarType, 4>{
        {ScalarType{0}, ScalarType{-1}, std::numeric_limits<ScalarType>::quiet_NaN(), ScalarType(1e-7)}};

    for (auto const dx : invalid_dx_values) {
        auto expected = std::stringstream();
        expected << "invalid grid spacing: " << fmm_test::ToString(fmm_test::FilledArray<kDimension>(dx));

        AssertThrowsWithMessage<std::invalid_argument>(
            [&]() { [[maybe_unused]] DistanceSolverType const solver(dx); }, expected.str());
    }
}

void SignedArrivalTimeTests::testDifferentUniformSpeed()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    boundary_indices.push_back(fmm_test::FilledArray<kDimension>(std::int32_t{5}));
    auto boundary_times = std::vector<ScalarType>();
    boundary_times.push_back(ScalarType{0});

    auto const speed1 = ScalarType{1};
    auto const speed2 = ScalarType{2};

    auto const times1 = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                               EikonalSolverType(grid_spacing, speed1));
    auto const times2 = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                               EikonalSolverType(grid_spacing, speed2));

    // Halving the speed should halve the distance everywhere.
    for (auto i = std::size_t{0}; i < times1.size(); ++i) {
        CPPUNIT_ASSERT(std::fabs(speed1 * times1[i] - speed2 * times2[i]) <= ScalarType(1e-3));
    }
}

void SignedArrivalTimeTests::testNonUniformGridSpacing()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{11});
    auto grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{0});
    for (auto i = std::size_t{0}; i < kDimension; ++i) {
        grid_spacing[i] = ScalarType(1) / static_cast<ScalarType>(i + 1);
    }

    auto const boundary_indices = std::vector<std::array<std::int32_t, kDimension>>(
        std::size_t{1}, fmm_test::FilledArray<kDimension>(std::int32_t{5}));
    auto const boundary_times = std::vector<ScalarType>(std::size_t{1}, ScalarType{0});

    auto const times = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                              EikonalSolverType(grid_spacing, ScalarType{1}));
    auto time_grid = fmm_test::Grid<ScalarType const, kDimension>(grid_size, *times.data());

    auto center = fmm_test::FilledArray<kDimension>(ScalarType{0});
    for (auto i = std::size_t{0}; i < kDimension; ++i) {
        center[i] = (boundary_indices[0][i] + ScalarType(0.5)) * grid_spacing[i];
    }

    // Point-source accuracy bound for 3D, from the upstream test suite
    // (based on Baerentzen's "On the implementation of fast marching
    // methods for 3D lattices").
    constexpr ScalarType kMaxAbsError = ScalarType(1.51);

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    bool has_next = true;
    while (has_next) {
        auto const index = index_iter.index();
        auto position = fmm_test::FilledArray<kDimension>(ScalarType{0});
        for (auto i = std::size_t{0}; i < kDimension; ++i) {
            position[i] = (index[i] + ScalarType(0.5)) * grid_spacing[i];
        }
        auto delta = fmm_test::FilledArray<kDimension>(ScalarType{0});
        for (auto i = std::size_t{0}; i < kDimension; ++i) {
            delta[i] = center[i] - position[i];
        }
        auto const ground_truth = fmm_test::Magnitude(delta);
        CPPUNIT_ASSERT(std::fabs(time_grid.Cell(index) - ground_truth) <= kMaxAbsError);
        has_next = index_iter.Next();
    }
}

void SignedArrivalTimeTests::testBoxBoundary()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    bool has_next = true;
    while (has_next) {
        auto const index = index_iter.index();
        for (auto i = std::size_t{0}; i < kDimension; ++i) {
            auto const edge = static_cast<std::int32_t>(grid_size[i]) - 1;
            if (index[i] == 0 || index[i] == edge) {
                boundary_indices.push_back(index);
                break;
            }
        }
        has_next = index_iter.Next();
    }
    auto const boundary_times = std::vector<ScalarType>(boundary_indices.size(), ScalarType{0});

    auto const times = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                              EikonalSolverType(grid_spacing, ScalarType{1}));

    // Everything is at or inside the box's boundary, so every signed
    // distance should be non-positive (inside/on-surface convention).
    for (auto const t : times) {
        CPPUNIT_ASSERT(t <= ScalarType{0});
    }
}

void SignedArrivalTimeTests::testCheckerboard()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto const is_boundary = [](std::array<std::int32_t, kDimension> const& index) {
        return std::accumulate(std::begin(index), std::end(index), std::int32_t{0}) % 2 == 0;
    };

    auto boundary_indices = std::vector<std::array<std::int32_t, kDimension>>();
    {
        auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
        bool has_next = true;
        while (has_next) {
            auto const index = index_iter.index();
            if (is_boundary(index)) {
                boundary_indices.push_back(index);
            }
            has_next = index_iter.Next();
        }
    }
    auto const boundary_times = std::vector<ScalarType>(boundary_indices.size(), ScalarType{0});

    auto const times = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                              EikonalSolverType(grid_spacing, ScalarType{1}));
    auto time_grid = fmm_test::Grid<ScalarType const, kDimension>(grid_size, *times.data());

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    bool has_next = true;
    while (has_next) {
        auto const index = index_iter.index();
        if (!is_boundary(index)) {
            bool is_edge = false;
            for (auto i = std::size_t{0}; i < kDimension; ++i) {
                auto const edge = static_cast<std::int32_t>(grid_size[i]) - 1;
                if (index[i] == 0 || index[i] == edge) {
                    is_edge = true;
                    break;
                }
            }
            auto const t = time_grid.Cell(index);
            // Checkerboard non-boundary cells at the grid edge are "outside"
            // (positive); everywhere else in the interior is "inside" a
            // boundary cell's face-neighborhood (negative).
            if (is_edge) {
                CPPUNIT_ASSERT(t > ScalarType{0});
            } else {
                CPPUNIT_ASSERT(t < ScalarType{0});
            }
        }
        has_next = index_iter.Next();
    }
}

void SignedArrivalTimeTests::testOverlappingBoxes()
{
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{16});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto const box_corner1 = fmm_test::FilledArray<kDimension>(std::int32_t{1});
    auto const box_size1 = fmm_test::FilledArray<kDimension>(std::size_t{10});
    auto const box_corner2 = fmm_test::FilledArray<kDimension>(std::int32_t{5});
    auto const box_size2 = fmm_test::FilledArray<kDimension>(std::size_t{10});

    auto indices1 = std::vector<std::array<std::int32_t, kDimension>>();
    auto times1 = std::vector<ScalarType>();
    fmm_test::BoxBoundaryCells(box_corner1, box_size1, grid_size, &indices1, &times1);
    auto indices2 = std::vector<std::array<std::int32_t, kDimension>>();
    auto times2 = std::vector<ScalarType>();
    fmm_test::BoxBoundaryCells(box_corner2, box_size2, grid_size, &indices2, &times2);

    // Merge, dropping duplicate indices (the boxes overlap).
    auto boundary_indices = indices1;
    auto boundary_times = times1;
    for (auto i = std::size_t{0}; i < indices2.size(); ++i) {
        if (std::find(std::begin(boundary_indices), std::end(boundary_indices), indices2[i]) ==
            std::end(boundary_indices)) {
            boundary_indices.push_back(indices2[i]);
            boundary_times.push_back(times2[i]);
        }
    }

    auto const times = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                              EikonalSolverType(grid_spacing, ScalarType{1}));
    auto time_grid = fmm_test::Grid<ScalarType const, kDimension>(grid_size, *times.data());

    // Cell 0 is outside both boxes; 2, 6, 14 are inside the union.
    CPPUNIT_ASSERT(time_grid.Cell(fmm_test::FilledArray<kDimension>(std::int32_t{0})) > ScalarType{0});
    CPPUNIT_ASSERT(time_grid.Cell(fmm_test::FilledArray<kDimension>(std::int32_t{2})) < ScalarType{0});
    CPPUNIT_ASSERT(time_grid.Cell(fmm_test::FilledArray<kDimension>(std::int32_t{6})) < ScalarType{0});
    CPPUNIT_ASSERT(time_grid.Cell(fmm_test::FilledArray<kDimension>(std::int32_t{14})) < ScalarType{0});
}

void SignedArrivalTimeTests::testPointSourceAccuracy()
{
    // Uses DistanceSolver specifically - this is the exact solver type
    // FGClouds::generateSDF()/SGVoxelTextureCloud::generateCloud() actually
    // instantiate, unlike the other ported tests above (UniformSpeedEikonal-
    // Solver), so this is the most direct check against real usage.
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{41});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});

    auto const boundary_indices = std::vector<std::array<std::int32_t, kDimension>>(
        std::size_t{1}, fmm_test::FilledArray<kDimension>(std::int32_t{20}));
    auto const boundary_times = std::vector<ScalarType>(std::size_t{1}, ScalarType{0});

    auto center = fmm_test::FilledArray<kDimension>(ScalarType{0});
    for (auto i = std::size_t{0}; i < kDimension; ++i) {
        center[i] = (boundary_indices[0][i] + ScalarType(0.5)) * grid_spacing[i];
    }

    auto const distances = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                                  DistanceSolverType(grid_spacing[0]));
    auto distance_grid = fmm_test::Grid<ScalarType const, kDimension>(grid_size, *distances.data());

    constexpr ScalarType kMaxAbsError = ScalarType(1.51);
    constexpr ScalarType kAvgAbsError = ScalarType(0.92);

    auto max_abs_error = ScalarType{0};
    auto sum_abs_error = ScalarType{0};
    auto n = std::size_t{0};

    auto index_iter = fmm_test::IndexIterator<kDimension>(grid_size);
    bool has_next = true;
    while (has_next) {
        auto const index = index_iter.index();
        auto position = fmm_test::FilledArray<kDimension>(ScalarType{0});
        for (auto i = std::size_t{0}; i < kDimension; ++i) {
            position[i] = (index[i] + ScalarType(0.5)) * grid_spacing[i];
        }
        auto delta = fmm_test::FilledArray<kDimension>(ScalarType{0});
        for (auto i = std::size_t{0}; i < kDimension; ++i) {
            delta[i] = center[i] - position[i];
        }
        auto const ground_truth = fmm_test::Magnitude(delta);
        // Only check within a radius of 20 - matches the upstream test,
        // which excludes the grid corners where the point source's
        // influence is least accurate.
        if (ground_truth <= ScalarType{20}) {
            auto const abs_error = std::fabs(distance_grid.Cell(index) - ground_truth);
            max_abs_error = std::max(max_abs_error, abs_error);
            sum_abs_error += abs_error;
            ++n;
        }
        has_next = index_iter.Next();
    }

    CPPUNIT_ASSERT(n > 0);
    CPPUNIT_ASSERT(max_abs_error <= kMaxAbsError);
    CPPUNIT_ASSERT((sum_abs_error / static_cast<ScalarType>(n)) <= kAvgAbsError);
}

void SignedArrivalTimeTests::testMaxDistanceCap()
{
    // A single point source in the middle of a modest cube - large enough
    // that a distance cap well inside the grid leaves both "reached" and
    // "never reached" cells to check.
    auto const grid_size = fmm_test::FilledArray<kDimension>(std::size_t{21});
    auto const grid_spacing = fmm_test::FilledArray<kDimension>(ScalarType{1});
    auto const boundary_indices = std::vector<std::array<std::int32_t, kDimension>>(
        std::size_t{1}, fmm_test::FilledArray<kDimension>(std::int32_t{10}));
    auto const boundary_times = std::vector<ScalarType>(std::size_t{1}, ScalarType{0});

    auto const uncapped = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                                 DistanceSolverType(grid_spacing[0]));

    constexpr ScalarType kMaxDistance = ScalarType{5};
    auto const capped = fmm::SignedArrivalTime(grid_size, boundary_indices, boundary_times,
                                               DistanceSolverType(grid_spacing[0]), kMaxDistance);

    CPPUNIT_ASSERT_EQUAL(uncapped.size(), capped.size());

    auto saw_within_cap = false;
    auto saw_beyond_cap = false;
    for (auto i = std::size_t{0}; i < uncapped.size(); ++i) {
        // Sanity check on the uncapped baseline itself: with no cap, every
        // cell must be frozen (a real value, not the pre-march sentinel).
        CPPUNIT_ASSERT(uncapped[i] < std::numeric_limits<ScalarType>::max());

        if (uncapped[i] <= kMaxDistance) {
            saw_within_cap = true;
            // The march computes cells in non-decreasing distance order, so
            // a cell frozen before the cap is exceeded is computed exactly
            // the same way whether or not the cap exists - bitwise equal,
            // not just approximately equal.
            CPPUNIT_ASSERT_EQUAL(uncapped[i], capped[i]);
        } else {
            saw_beyond_cap = true;
            // Never reached: still at the pre-march sentinel.
            CPPUNIT_ASSERT_EQUAL(std::numeric_limits<ScalarType>::max(), capped[i]);
        }
    }

    // Both branches above should actually have been exercised, or this test
    // isn't testing what it claims to.
    CPPUNIT_ASSERT(saw_within_cap);
    CPPUNIT_ASSERT(saw_beyond_cap);
}
