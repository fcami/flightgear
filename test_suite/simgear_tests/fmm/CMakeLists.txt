# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 Stuart Buchanan

target_sources(fgfs_test_suite PRIVATE
    TestSuite.cxx
    test_signed_arrival_time.cxx
    test_signed_arrival_time.hxx
    fmm_test_util.hxx
)

fg_add_test_suite(SignedArrivalTimeTests m)
