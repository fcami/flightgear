# SPDX-FileCopyrightText: (C) 2026 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_tileEntry.cxx
    test_tileEntry.hxx
)

fg_add_test_suite(TileEntryTests u)
