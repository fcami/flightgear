// AptDatConstants - Constants from the realm of APT.DAT
//
// SPDX-FileCopyrightText: Copyright (C) 2026 Keith Paterson <keith.paterson@gmx.de>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

using namespace std::literals::string_view_literals;

namespace flightgear {
/*
	* Categories from APT.dat
	* Pipe-separated list ("|"). Can include "heavy", "jets", "turboprops", "props" and "helos"
	*/
inline constexpr auto PERFORMANCE_CLASS_HEAVY = "heavy"sv;
inline constexpr auto PERFORMANCE_CLASS_JETS = "jets"sv;
inline constexpr auto PERFORMANCE_CLASS_TURBOPROPS = "turboprops"sv;
inline constexpr auto PERFORMANCE_CLASS_PROPS = "props"sv;
inline constexpr auto PERFORMANCE_CLASS_HELOS = "helos"sv;
inline constexpr auto PERFORMANCE_CLASS_FIGHTERS = "fighters"sv;
inline constexpr auto PERFORMANCE_CLASS_BALLOON = "balloon"sv;
inline constexpr auto PERFORMANCE_CLASS_SEAPLANE = "seaplane"sv;
inline constexpr auto PERFORMANCE_CLASS_GLIDER = "glider"sv;
inline constexpr auto PERFORMANCE_CLASS_GROUNDVEHICLE = "groundvehicle"sv;
inline constexpr auto PERFORMANCE_CLASS_SHIP = "ship"sv;
} // namespace flightgear
