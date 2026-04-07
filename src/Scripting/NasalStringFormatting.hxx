// SPDX-FileCopyrightText: 2026 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief String formatting function for Nasal similar to std::format()
 *
 * Synopsis: format(fmt_string, args...)
 */

#pragma once

namespace nasal {

/**
 * Add the new format() function to the global Nasal namespace.
 */
void addStringFormattingFunction();

} // namespace nasal
