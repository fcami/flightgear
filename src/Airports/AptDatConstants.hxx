// AptDatConstants - Constants from the realm of APT.DAT
//
// SPDX-FileCopyrightText: Copyright (C) 2026 Keith Paterson <keith.paterson@gmx.de>
// SPDX-License-Identifier: GPL-2.0-or-later

namespace flightgear
{
	/*
	* Categories from APT.dat
	* Pipe-separated list (“|”). Can include “heavy”, ”jets”, “turboprops”, “props” and “helos”
	*/
	inline constexpr std::string_view PERFORMANCE_CLASS_HEAVY = "heavy";
	inline constexpr std::string_view PERFORMANCE_CLASS_JETS = "jets";
	inline constexpr std::string_view PERFORMANCE_CLASS_TURBOPROPS = "turboprops";
	inline constexpr std::string_view PERFORMANCE_CLASS_PROPS = "props";
	inline constexpr std::string_view PERFORMANCE_CLASS_HELOS = "helos";
	inline constexpr std::string_view PERFORMANCE_CLASS_FIGHTERS = "fighters";
	inline constexpr std::string_view PERFORMANCE_CLASS_BALLOON = "balloon";
	inline constexpr std::string_view PERFORMANCE_CLASS_SEAPLANE = "seaplane";
	inline constexpr std::string_view PERFORMANCE_CLASS_GLIDER = "glider";
	inline constexpr std::string_view PERFORMANCE_CLASS_GROUNDVEHICLE = "groundvehicle";
	inline constexpr std::string_view PERFORMANCE_CLASS_SHIP = "ship";
}