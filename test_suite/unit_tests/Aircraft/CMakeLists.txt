# SPDX-FileCopyrightText: (C) 2025  James Turner <james@flightgear.org>
# SPDX-License-Identifier: GPL-2.0-or-later

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx test_controls.cxx test_history.cxx
    test_controls.hxx test_history.hxx
)

fg_add_test_suite(ControlsTests u)
fg_add_test_suite(HistoryTests  u)
