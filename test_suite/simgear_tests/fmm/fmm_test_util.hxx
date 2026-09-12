// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Tommy Hinks
//
// Trimmed and adapted from the upstream thinks::fast_marching_method test
// utilities (https://github.com/thinks/fast-marching-method,
// tests/util.hpp) to just what the ported tests in this directory need.
// Kept in its own namespace (fmm_test) rather than reusing "util" to avoid
// colliding with any other test_suite helper of that name.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace fmm_test {

//! Returns array @a as a string, e.g. "[1, 2, 3]" - matches the format used
//! by fast_marching_method.hpp's own internal ToString(), which is what
//! populates the exception messages the throw-tests below check against.
template <typename T, std::size_t N>
std::string ToString(std::array<T, N> const& a)
{
    auto ss = std::stringstream();
    ss << "[";
    for (auto i = std::size_t{0}; i < N; ++i) {
        ss << a[i];
        if (i != (N - 1)) {
            ss << ", ";
        }
    }
    ss << "]";
    return ss.str();
}

//! Returns an array with all elements initialized to @a v.
template <std::size_t N, typename T>
inline std::array<T, N> FilledArray(T const v)
{
    auto r = std::array<T, N>{};
    std::fill(std::begin(r), std::end(r), v);
    return r;
}

//! Returns the product of the elements in @a a.
template <std::size_t N>
inline std::size_t LinearSize(std::array<std::size_t, N> const& a)
{
    return std::accumulate(std::begin(a), std::end(a), std::size_t{1}, std::multiplies<std::size_t>());
}

//! Returns the magnitude of vector @a v.
template <typename T, std::size_t N>
T Magnitude(std::array<T, N> const& v)
{
    auto mag_squared = T{0};
    for (auto i = std::size_t{0}; i < N; ++i) {
        mag_squared += v[i] * v[i];
    }
    return std::sqrt(mag_squared);
}

//! Iterates every index in an N-dimensional grid of the given size, in
//! row-major order starting at the origin. See fast_marching_method.hpp's
//! own MarchNarrowBand for the grid indexing convention this mirrors.
template <std::size_t N>
class IndexIterator
{
public:
    explicit IndexIterator(std::array<std::size_t, N> const& size)
        : size_(size), index_(FilledArray<N>(std::int32_t{0})), has_next_(true)
    {
    }

    std::array<std::int32_t, N> index() const { return index_; }

    bool has_next() const { return has_next_; }

    bool Next()
    {
        auto i = std::size_t{0};
        while (i < N) {
            if (static_cast<std::size_t>(index_[i] + 1) < size_[i]) {
                ++index_[i];
                return true;
            } else {
                index_[i++] = 0;
            }
        }
        has_next_ = false;
        return false;
    }

private:
    std::array<std::size_t, N> const size_;
    std::array<std::int32_t, N> index_;
    bool has_next_;
};

//! Accesses a linear buffer as if it were an N-dimensional grid (no range
//! checking) - used to read back fmm::SignedArrivalTime's flat result
//! vector by grid coordinate instead of linear index.
template <typename T, std::size_t N>
class Grid
{
public:
    typedef std::array<std::size_t, N> SizeType;
    typedef std::array<std::int32_t, N> IndexType;

    Grid(SizeType const& size, T& cells) : size_(size), cells_(&cells)
    {
        auto stride = std::size_t{1};
        for (auto i = std::size_t{1}; i < N; ++i) {
            stride *= size_[i - 1];
            strides_[i - 1] = stride;
        }
    }

    T const& Cell(IndexType const& index) const { return cells_[LinearIndex_(index)]; }

private:
    std::size_t LinearIndex_(IndexType const& index) const
    {
        auto k = static_cast<std::size_t>(index[0]);
        for (auto i = std::size_t{1}; i < N; ++i) {
            k += static_cast<std::size_t>(index[i]) * strides_[i - 1];
        }
        return k;
    }

    std::array<std::size_t, N> const size_;
    std::array<std::size_t, N - 1> strides_;
    T* const cells_;
};

//! Fills @a boundary_indices/@a boundary_distances with the cells on the
//! surface of an axis-aligned box (all distances zero) - matches
//! util.hpp's BoxBoundaryCells(), trimmed to drop the templated distance
//! type parameter (the ported tests only ever use float).
template <std::size_t N>
void BoxBoundaryCells(std::array<std::int32_t, N> const& box_corner,
                      std::array<std::size_t, N> const& box_size,
                      std::array<std::size_t, N> const& grid_size,
                      std::vector<std::array<std::int32_t, N>>* boundary_indices,
                      std::vector<float>* boundary_distances)
{
    auto box_min = box_corner;
    auto box_max = box_corner;
    for (auto i = std::size_t{0}; i < N; ++i) {
        box_max[i] += static_cast<std::int32_t>(box_size[i]);
    }

    auto index_iter = IndexIterator<N>(grid_size);
    bool has_next = true;
    while (has_next) {
        auto const index = index_iter.index();
        bool inside = true;
        for (auto i = std::size_t{0}; i < N; ++i) {
            if (!(box_min[i] <= index[i] && index[i] <= box_max[i])) {
                inside = false;
                break;
            }
        }
        if (inside) {
            for (auto i = std::size_t{0}; i < N; ++i) {
                if (index[i] == box_min[i] || index[i] == box_max[i]) {
                    boundary_indices->push_back(index);
                    break;
                }
            }
        }
        has_next = index_iter.Next();
    }

    *boundary_distances = std::vector<float>(boundary_indices->size(), 0.0f);
}

} // namespace fmm_test
