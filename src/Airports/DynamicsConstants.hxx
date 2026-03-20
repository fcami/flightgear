// DynamicsConstants
//
// SPDX-FileCopyrightText: Copyright (C) 2026 Keith Paterson <keith.paterson@gmx.de>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

using namespace std::literals::string_view_literals;

namespace flightgear {
/*
	* Categories from rwyuse
	*/
inline constexpr auto RUNWAY_TYPE_COM = "com"sv;
inline constexpr auto RUNWAY_TYPE_GEN = "gen"sv;
inline constexpr auto RUNWAY_TYPE_UL = "ul"sv;
inline constexpr auto RUNWAY_TYPE_MIL = "mil"sv;
} // namespace flightgear
